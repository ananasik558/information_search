from pymongo import MongoClient, ASCENDING
from pymongo.errors import DuplicateKeyError

def get_db(config):
    client = MongoClient(host=config['host'], port=config['port'])
    db = client[config['name']]
    collection = db[config['collection']]
    collection.create_index([("url", ASCENDING)], unique=True)
    return collection

def document_exists(collection, url):
    return collection.count_documents({"url": url}, limit=1) > 0

def get_document_metadata(collection, url):
    return collection.find_one(
        {"url": url},
        {"last_modified": 1, "etag": 1, "content_hash": 1, "_id": 0}
    )

def save_document(collection, data):
    try:
        collection.insert_one(data)
    except DuplicateKeyError:
        update_document(collection, data["url"], data)

def update_document(collection, url, data):
    collection.update_one({"url": url}, {"$set": data})