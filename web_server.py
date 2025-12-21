# web_server.py
from flask import Flask, request, render_template_string
import subprocess
import sys

app = Flask(__name__)

HOME_TEMPLATE = """
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>Поисковая система</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        form { margin-bottom: 20px; }
        input[type="text"] { width: 60%; padding: 8px; }
        input[type="submit"] { padding: 8px 16px; }
        .result { margin-bottom: 15px; }
        .pagination { margin-top: 20px; }
    </style>
</head>
<body>
    <h1>Поисковая система</h1>
    <form method="GET" action="/search">
        <input type="text" name="q" placeholder="Введите запрос..." value="{{ query|default('', true) }}" required>
        <input type="submit" value="Найти">
    </form>
    {% if results %}
        <h2>Результаты поиска ({{ start+1 }}–{{ end }} из {{ total }})</h2>
        {% for title, url in results %}
            <div class="result">
                <a href="{{ url }}">{{ title }}</a>
            </div>
        {% endfor %}
        <div class="pagination">
            {% if start > 0 %}
                <a href="?q={{ query|urlencode }}&offset={{ start-50 }}">Предыдущие 50</a> |
            {% endif %}
            {% if end < total %}
                <a href="?q={{ query|urlencode }}&offset={{ end }}">Следующие 50</a>
            {% endif %}
        </div>
    {% elif query %}
        <p>Ничего не найдено.</p>
    {% endif %}
</body>
</html>
"""

RESULTS_TEMPLATE = """
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <title>Результаты поиска</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .result { margin-bottom: 15px; }
        .pagination { margin-top: 20px; }
    </style>
</head>
<body>
    <h1>Результаты поиска: "{{ query }}"</h1>
    {% if results %}
        <h2>Найдено {{ total }} документов</h2>
        {% for title, url in results %}
            <div class="result">
                <a href="{{ url }}">{{ title }}</a>
            </div>
        {% endfor %}
        <div class="pagination">
            {% if start > 0 %}
                <a href="?q={{ query|urlencode }}&offset={{ start-50 }}">Предыдущие 50</a> |
            {% endif %}
            {% if end < total %}
                <a href="?q={{ query|urlencode }}&offset={{ end }}">Следующие 50</a>
            {% endif %}
        </div>
    {% else %}
        <p>Ничего не найдено.</p>
    {% endif %}
    <a href="/">Новый запрос</a>
</body>
</html>
"""

@app.route('/')
def home():
    return render_template_string(HOME_TEMPLATE)

@app.route('/search')
def search():
    query = request.args.get('q', '').strip()
    offset = int(request.args.get('offset', 0))

    if not query:
        return render_template_string(HOME_TEMPLATE, query=query)

    try:
        result = subprocess.run(
            ["./search", query],
            capture_output=True,
            text=True,
            encoding='utf-8',
            check=True
        )
        output_lines = result.stdout.splitlines()
        results = []
        for line in output_lines:
            if " | " in line:
                parts = line.split(" | ", 1)
                if len(parts) == 2:
                    title, url = parts[0], parts[1]
                    results.append((title, url))
    except Exception as e:
        results = []
        print(f"Ошибка при выполнении поиска: {e}")

    start = offset
    end = min(start + 50, len(results))
    paginated_results = results[start:end]

    return render_template_string(RESULTS_TEMPLATE,
                                 query=query,
                                 results=paginated_results,
                                 start=start,
                                 end=end,
                                 total=len(results))

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)