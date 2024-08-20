import base64
import hashlib
import os
from pathlib import Path
import time
import uuid
import numpy as np
import traceback
import redis
import torch
import torchvision
from torchvision.transforms import v2
from ml_dtypes import bfloat16

import tqdm
from fastapi import FastAPI, Request, HTTPException

from redis.commands.search.field import VectorField as RedisVectorField, TagField as RedisTagField,  GeoField as RedisGeoField
from redis.commands.search.indexDefinition import IndexDefinition as RedisIndexDefinition, IndexType as RedisIndexType
from redis.commands.search.query import Query as RedisQuery

class Sample():
    Lon: float
    Lat: float
    Altitude: float = 0.0
    HeadingAngle: float = 0.0
    Descriptor: bytes = []
    ImageFilePath: Path = ""
    DatasetName: str = ""
    ImageData: torch.Tensor = []

class SaladWrapper(torch.nn.Module):
    def __init__(self):
        super().__init__()
        self.model = torch.hub.load("serizba/salad", "dinov2_salad")
        self.resize = v2.Resize([322, 322], antialias=True)
    def forward(self, images):
        images = self.resize(images)
        return self.model(images)

descriptor_dim = 8448

model_real = SaladWrapper()
model_real.desc_dim = descriptor_dim
model_real.eval()

model_synth = SaladWrapper()
model_synth.desc_dim = descriptor_dim
model_synth.eval()

weight_path = Path(__file__).parent.resolve() / "best_model.pth"

model_synth_state_dict = torch.load(weight_path, map_location=torch.device('cpu'))
model_synth.load_state_dict(model_synth_state_dict)

use_cuda = torch.cuda.is_available()

if use_cuda:
    print("moving to cuda")
    model_real = model_real.cuda()
    model_synth = model_synth.cuda()
else:
    print("running on cpu")

base_transform = v2.Compose([
        v2.Resize([512, 512]),
        v2.ToDtype(torch.float32, scale=True),
        v2.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225], inplace=True),
    ])

thumbnail_transform = v2.Compose([
        v2.Resize([224, 224]),
        v2.Normalize(mean=[-0.485/0.229, -0.456/0.224, -0.406/0.225], std=[1/0.229, 1/0.224, 1/0.225], inplace=True),
        v2.ToDtype(torch.uint8, scale=True),
    ])

app = FastAPI()

REDIS_HOST = os.getenv("REDIS_HOST", "127.0.0.1")

redis_client = redis.Redis(host=REDIS_HOST, port=6379, protocol=3, decode_responses=True, password="meshvpr,cvpr,2024")
# increase redis search timeout
redis_client.ft().config_set("TIMEOUT", 20000) # 10 seconds

redis_client_binary = redis.Redis(host=REDIS_HOST, port=6379, protocol=3, decode_responses=False, password="meshvpr,cvpr,2024")
# increase redis search timeout
redis_client_binary.ft().config_set("TIMEOUT", 20000) # 10 seconds

def redis_upload_sample_batch(redis_client: redis.Redis, samples: list[Sample]):
    redis_pipeline = redis_client.pipeline(transaction=False)
    for sample in samples:
        sample_identifier = build_unique_sample_id(sample)
        sample_dict = {}
        sample_dict["LonLat"] = f"{sample.Lon:.8f},{sample.Lat:.8f}"
        sample_dict["HeadingAngle"] = f"{sample.HeadingAngle:.8f}"
        sample_dict["DatasetName"] = sample.DatasetName
        sample_dict["Altitude"] = f"{sample.Altitude:.8f}"
        sample_dict["Descriptor"] = sample.Descriptor
        # Convert ImageData to 256x256 compressed jpeg
        thumbnail = thumbnail_transform(sample.ImageData)
        thumbnail = torchvision.io.encode_jpeg(thumbnail, quality=70).numpy().tobytes()
        sample_dict["Thumbnail"] = thumbnail #base64.b64encode(thumbnail).decode("utf-8")

        redis_pipeline.hset(f"geolocator:sample:{sample_identifier}", mapping=sample_dict)
        redis_pipeline.sadd("geolocator:datasets:names", sample.DatasetName)

    redis_pipeline.execute()

def get_descriptor_index_name():
    return "geolocator:idx:sample:descriptor"

def get_geo_index_name():
    return "geolocator:idx:sample:geo"

def redis_setup_descriptor_index(redis_client: redis.Redis):
    descriptor_schema = (RedisVectorField("Descriptor", "HNSW", {"TYPE": "BFLOAT16", "DIM": descriptor_dim, "DISTANCE_METRIC": "L2", "EF_RUNTIME": "256", "EF_CONSTRUCTION": "128", "M": "32"}), RedisTagField("DatasetName"))
    try:
        redis_client.ft(get_descriptor_index_name()).create_index(descriptor_schema, definition=RedisIndexDefinition(prefix=["geolocator:sample:"], index_type=RedisIndexType.HASH))
    except:
        pass

    wait_for_indexing_to_complete(redis_client, get_descriptor_index_name())


def redis_setup_geo_index(redis_client: redis.Redis):
    descriptor_schema = (RedisGeoField("LonLat"), RedisTagField("DatasetName"))
    try:
        redis_client.ft(get_geo_index_name()).create_index(descriptor_schema, definition=RedisIndexDefinition(prefix=["geolocator:sample:"], index_type=RedisIndexType.HASH))
    except:
        pass

    wait_for_indexing_to_complete(redis_client, get_geo_index_name())


def wait_for_indexing_to_complete(redis_client: redis.Redis, index_name: str):
    time.sleep(1)

    with tqdm.tqdm(total=100, desc=f"building {index_name}", ncols=100) as pbar:
        status = redis_client.ft(index_name).info()
        prev_percent_indexed = round(status["percent_indexed"] * 100, 2)
        pbar.update(prev_percent_indexed)

        while True:
            status = redis_client.ft(index_name).info()
            current_percent_indexed = round(status["percent_indexed"] * 100, 2)
            pbar.update(current_percent_indexed - prev_percent_indexed)
            prev_percent_indexed = current_percent_indexed

            if status["indexing"] == 0:
                return
            else:
                # sleep for one second
                time.sleep(1)

redis_setup_descriptor_index(redis_client)
redis_setup_geo_index(redis_client)

@app.post("/search-image")
async def search_image(request: Request):
    json_data = await request.json()
    limit = json_data["limit"]
    offset = json_data.get("offset", 0)
    dataset_names = json_data.get("dataset_names", [])

    vector_query = (
        RedisQuery(f"@DatasetName:{{{'|'.join(dataset_names)}}}=>[KNN $K @Descriptor $desc]")
        .paging(offset, limit)
        .dialect(2)
        .sort_by("__Descriptor_score")
        .return_fields("Thumbnail", "LonLat", "Altitude", "HeadingAngle", "DatasetName", "__Descriptor_score")
    )

    img = None
    try:
        img = torchvision.io.decode_image(torch.frombuffer(base64.b64decode(json_data['image']), dtype=torch.uint8))
        img = base_transform(img)
        img = img.unsqueeze(0)
    except Exception as _:
        traceback.print_exc()
        return HTTPException(status_code=400, detail="Invalid image")

    print("Searchin in region: ", json_data['dataset_names'])

    try:
        with torch.inference_mode():
            print("Running Inference")
            if use_cuda:
                img = img.cuda()
            descriptor: torch.FloatTensor = model_real(img)[0]
            if use_cuda:
                descriptor = descriptor.cpu()


            vector_result = redis_client_binary.ft(get_descriptor_index_name()).search(
                vector_query, query_params={"desc": tensor_to_redis_bytes(descriptor), "K": limit}
            )

            result_arr = []
            for result in vector_result[b'results']:
                doc = result[b'extra_attributes']

                result_arr.append({
                    "thumbnail": base64.b64encode(doc[b"Thumbnail"]).decode('utf-8'),
                    "lon": float(doc[b"LonLat"].decode('utf-8').split(",")[0]),
                    "lat": float(doc[b"LonLat"].decode('utf-8').split(",")[1]),
                    "altitude": float(doc[b"Altitude"].decode('utf-8')),
                    "heading_angle": float(doc[b"HeadingAngle"].decode('utf-8')),
                    "dataset_name": doc[b"DatasetName"].decode('utf-8'),
                    "score": float(doc[b"__Descriptor_score"].decode('utf-8'))
                })

            return {"result": result_arr}
    except Exception as _:
        traceback.print_exc()
        return HTTPException(status_code=500, detail="Internal Server Error")
    
@app.post("/process-pano")
async def process_pano(request: Request):
    # Request contains an img_path
    json_data = await request.json()
    # TODO pass metadata viafields isntead of path
    img_path = Path(json_data["image_path"])

    img = torchvision.io.decode_image(torch.frombuffer(base64.b64decode(json_data['image']), dtype=torch.uint8))

    # Process the image
    process_sample(img_path, img, img_path.parent / "processed", json_data["dataset_name"])

@app.get("/get-datasets")
async def get_datasets(request: Request):
    return {"datasets": redis_client.smembers("geolocator:datasets:names")}

PANO_WIDTH = 3328
HEADING_INDEX = 9
ALTITUDE_INDEX = 12
LONGITUDE_INDEX = 6
LATITUDE_INDEX = 5

def process_sample(pano_path, pano_tensor, out_dir, dataset_name):
    pano_name = Path(pano_path).name

    pano_heading = float(pano_name.split("@")[HEADING_INDEX])
    assert 0 <= pano_heading <= 360
    north_heading_in_degrees = (180-pano_heading) % 360

    heading_offset_in_pixels = int((north_heading_in_degrees / 360) * PANO_WIDTH)

    pano_tensor = torch.cat([pano_tensor, pano_tensor], 2)
    assert pano_tensor.shape == torch.Size([3, 512, PANO_WIDTH*2]), f"Pano {pano_path} has wrong shape: {pano_tensor.shape}"

    samples: list[Sample] = []
    
    for crop_heading in range(0, 360, 30):
        
        crop_offset_in_pixels = int(crop_heading / 360 * PANO_WIDTH)
        
        offset = (heading_offset_in_pixels + crop_offset_in_pixels) % PANO_WIDTH
        crop = pano_tensor[:, :, offset : offset+512]
        assert crop.shape == torch.Size([3, 512, 512]), f"Crop {pano_path} has wrong shape: {crop.shape}"

        fov_half = 512 / PANO_WIDTH * 360 / 2
        crop_heading_center = crop_heading + fov_half
        crop_filename = "@" + "@".join(pano_name.split("@")[1:HEADING_INDEX] + ["{:.2f}".format(crop_heading_center)] + pano_name.split("@")[HEADING_INDEX+1:])

        crop_absolute_path = Path(out_dir) / "{:.2f}".format(float(pano_name.split("@")[LATITUDE_INDEX])) / crop_filename
        
        crop_absolute_path.parent.mkdir(parents=True, exist_ok=True)

        if not crop_absolute_path.exists():
            torchvision.io.write_jpeg(crop, str(crop_absolute_path))

        sample = Sample()
        sample.Lon=float(pano_name.split("@")[LONGITUDE_INDEX])
        sample.Lat=float(pano_name.split("@")[LATITUDE_INDEX])
        sample.HeadingAngle=crop_heading_center
        sample.Altitude=float(pano_name.split("@")[ALTITUDE_INDEX])
        sample.ImageFilePath=crop_absolute_path
        sample.DatasetName=dataset_name
        sample.ImageData=base_transform(crop)

        samples.append(
            sample
        )
    
    with torch.inference_mode():
        batch = torch.stack([sample.ImageData for sample in samples])
        if use_cuda:
            batch = batch.cuda()
        descriptors = model_synth(batch)
        if use_cuda:
            descriptors = descriptors.cpu()

        for i, sample in enumerate(samples):
            sample.Descriptor = tensor_to_redis_bytes(descriptors[i])

        redis_upload_sample_batch(redis_client, samples)

def build_unique_sample_id(sample: Sample):
    sample_file_path_hash = hashlib.blake2b(str(sample.ImageFilePath).encode("utf-8"), digest_size=8).hexdigest()
    # this is 99% guaranteed to be non colliding,
    # would require collision in file name hash and lat lon coords and heading angle
    sample_identifier = f"geolocator:{sample_file_path_hash}.{sample.Lon:.8f}.{sample.Lat:.8f}.{sample.HeadingAngle:.8f}"

    return str(uuid.uuid5(uuid.NAMESPACE_URL, sample_identifier))

def tensor_to_redis_bytes(tensor: torch.Tensor):
    return tensor.numpy().astype(bfloat16).tobytes()
    
