import os
import sys
import yaml
import requests
from bs4 import BeautifulSoup
import time
import json
import hashlib
from information_search.db import get_db, document_exists, get_document_metadata, save_document, update_document

DEFAULT_CONFIG_PATH = "config.yaml"
CORPUS_DIR = "corpus" 
MAX_DOCS = 50000
REQUEST_DELAY = 1.5
USER_AGENT = "MAI-CarCrawler/1.0 (student@mai.ru)"
SESSION = requests.Session()
SESSION.headers.update({"User-Agent": USER_AGENT})


def extract_wikipedia_text(html: str) -> str:
    soup = BeautifulSoup(html, "html.parser")
    for tag in soup([
        "script", "style", "nav", "footer", "aside",
        "table.infobox", "div.navbox", "div.thumb", "div.metadata"
    ]):
        tag.decompose()
    content = soup.find("div", {"class": "mw-parser-output"})
    if content:
        return content.get_text(separator=" ", strip=True)
    return ""


def extract_autoru_text(html: str) -> str:
    soup = BeautifulSoup(html, "html.parser")
    for selector in ["script", "style", "nav", "footer", "aside", "header",
                     ".b-popup", ".b-banner", ".ListingItem", ".ListingItems"]:
        for el in soup.select(selector):
            el.decompose()
    content = soup.select_one(".article")
    if content:
        return content.get_text(separator=" ", strip=True)
    return ""


def fetch_document(url: str):
    try:
        resp = SESSION.get(url, timeout=20)
        if resp.status_code == 200:
            return resp.text, resp.headers
        else:
            print(f"HTTP {resp.status_code} для {url}")
    except Exception as e:
        print(f" Ошибка загрузки {url}: {e}")
    return None, None


def normalize_url(source: str, title: str) -> str:
    if source == "wikipedia":
        clean_title = title.replace(' ', '_')
        return f"https://en.wikipedia.org/wiki/{clean_title}"
    elif source == "autoru":
        return f"https://auto.ru/articles/{title}/"
    else:
        raise ValueError(f"Неизвестный источник: {source}")


def main():
    if len(sys.argv) != 2:
        print("Использование: python crawler.py <путь_к_config.yaml>")
        sys.exit(1)

    config_path = sys.argv[1]
    with open(config_path, "r", encoding="utf-8") as f:
        config = yaml.safe_load(f)

    collection = get_db(config["db"])

    titles_file = config.get("titles_file", "data/titles_en.txt")
    with open(titles_file, "r", encoding="utf-8") as f:
        titles = [line.strip() for line in f if line.strip()]
    print(f"Найдено заголовков: {len(titles)}")

    max_docs = config.get("logic", {}).get("max_docs", MAX_DOCS)
    request_delay = config.get("logic", {}).get("delay", REQUEST_DELAY)

    saved = 0
    for full_title in titles:
        if saved >= max_docs:
            break

        if "::" not in full_title:
            print(f"Пропускаем: {full_title}")
            continue

        source, title = full_title.split("::", 1)
        try:
            url = normalize_url(source, title)
        except ValueError as e:
            print(e)
            continue

        if document_exists(collection, url):
            meta = get_document_metadata(collection, url)
            stored_lm = meta.get("last_modified")
            stored_etag = meta.get("etag")

            try:
                head_resp = SESSION.head(url, timeout=10)
                if head_resp.status_code != 200:
                    print(f"HEAD failed: {head_resp.status_code}")
                    continue

                current_lm = head_resp.headers.get("Last-Modified")
                current_etag = head_resp.headers.get("ETag")

                if (stored_lm and stored_lm == current_lm) or (stored_etag and stored_etag == current_etag):
                    print(f"Не изменился: {title}")
                    saved += 1
                    continue
            except Exception as e:
                print(f"Ошибка при проверке обновления: {e}")

        html, headers = fetch_document(url)
        if not html:
            continue

        if source == "wikipedia":
            text = extract_wikipedia_text(html)
        elif source == "autoru":
            text = extract_autoru_text(html)
        else:
            text = ""

        if len(text.split()) < 100:
            print(f" Слишком короткий текст: {title}")
            continue

        content_hash = hashlib.md5(text.encode("utf-8")).hexdigest()
        doc_data = {
            "url": url,
            "title": title,
            "source": source,
            "raw_html": html,
            "cleaned_text": text,
            "fetch_time": int(time.time()),
            "last_modified": headers.get("Last-Modified"),
            "etag": headers.get("ETag"),
            "content_hash": content_hash
        }

        if document_exists(collection, url):
            update_document(collection, url, doc_data)
            print(f"[{saved+1}] Обновлен: {title}")
        else:
            save_document(collection, doc_data)
            print(f"[{saved+1}] Сохранён: {title}")

        saved += 1
        time.sleep(request_delay)

    print(f"\nЗавершено. Всего документов: {saved}")


if __name__ == "__main__":
    main()