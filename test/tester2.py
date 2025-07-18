import requests
import os

# --- Configuración ---
BASE_URL = "http://127.0.0.1:8081"
# Crea un fichero de prueba para subir
TEST_UPLOAD_FILENAME = "test_upload_file.txt"
with open(TEST_UPLOAD_FILENAME, "w") as f:
    f.write("Este es un fichero de prueba para el upload.")

# --- Funciones de prueba ---

def print_test_header(title):
    print("\n" + "="*10 + f" {title} " + "="*10)

def test_static_get():
    """Prueba que se pueda obtener un fichero estático existente (new-page.html)."""
    print_test_header("Test GET Fichero Estático")
    url = f"{BASE_URL}/new-page.html"
    try:
        response = requests.get(url)
        print(f"Petición a: {url}")
        print(f"Respuesta: {response.status_code}")
        if response.status_code == 200:
            print("✅ PASS: El servidor devolvió 200 OK.")
        else:
            print(f"❌ FAIL: Se esperaba 200, se obtuvo {response.status_code}.")
    except requests.exceptions.ConnectionError as e:
        print(f"❌ FAIL: No se pudo conectar al servidor. {e}")

def test_not_found():
    """Prueba que el servidor devuelva 404 para un fichero inexistente."""
    print_test_header("Test Error 404 Not Found")
    url = f"{BASE_URL}/fichero-que-no-existe.html"
    try:
        response = requests.get(url)
        print(f"Petición a: {url}")
        print(f"Respuesta: {response.status_code}")
        if response.status_code == 404:
            print("✅ PASS: El servidor devolvió 404 Not Found como se esperaba.")
        else:
            print(f"❌ FAIL: Se esperaba 404, se obtuvo {response.status_code}.")
    except requests.exceptions.ConnectionError as e:
        print(f"❌ FAIL: No se pudo conectar al servidor. {e}")

def test_redirection():
    """Prueba que la redirección de /old-page funciona correctamente."""
    print_test_header("Test Redirección 301")
    url = f"{BASE_URL}/old-page"
    try:
        # allow_redirects=False para que la librería no siga la redirección automáticamente
        response = requests.get(url, allow_redirects=False)
        print(f"Petición a: {url}")
        print(f"Respuesta: {response.status_code}")
        print(f"Cabeceras: {response.headers}")

        location_header = response.headers.get('Location')
        if response.status_code == 301 and location_header == '/new-page.html':
            print("✅ PASS: Redirección 301 a /new-page.html correcta.")
        else:
            print(f"❌ FAIL: Respuesta de redirección incorrecta.")
    except requests.exceptions.ConnectionError as e:
        print(f"❌ FAIL: No se pudo conectar al servidor. {e}")

def test_cgi_get():
    """Prueba una petición GET a un script CGI."""
    print_test_header("Test GET a CGI")
    # Nota: Asume que tienes un script llamado 'test.py' en tu directorio cgi-bin
    url = f"{BASE_URL}/cgi-bin/test.py"
    try:
        response = requests.get(url)
        print(f"Petición a: {url}")
        print(f"Respuesta: {response.status_code}")
        if response.status_code == 200:
            print("✅ PASS: El script CGI respondió con 200 OK.")
            print("Cuerpo de la respuesta (parcial): " + response.text[:100] + "...")
        else:
            print(f"❌ FAIL: Se esperaba 200, se obtuvo {response.status_code}.")
    except requests.exceptions.ConnectionError as e:
        print(f"❌ FAIL: No se pudo conectar al servidor. {e}")

def test_upload_post():
    """Prueba subir un fichero vía POST a /uploads."""
    print_test_header("Test POST para Subir Fichero")
    url = f"{BASE_URL}/uploads"
    files = {'file': open(TEST_UPLOAD_FILENAME, 'rb')}
    try:
        response = requests.post(url, files=files)
        print(f"Petición a: {url} con el fichero {TEST_UPLOAD_FILENAME}")
        print(f"Respuesta: {response.status_code}")
        if response.status_code == 200 or response.status_code == 201:
            print("✅ PASS: El servidor aceptó el fichero con un código 200 o 201.")
        else:
            print(f"❌ FAIL: Se esperaba 200 o 201, se obtuvo {response.status_code}.")
    except requests.exceptions.ConnectionError as e:
        print(f"❌ FAIL: No se pudo conectar al servidor. {e}")
    finally:
        files['file'].close()
        os.remove(TEST_UPLOAD_FILENAME) # Limpia el fichero de prueba

# --- Ejecución de todas las pruebas ---
if __name__ == "__main__":
    print("Iniciando pruebas de regresión para el servidor web...")
    test_static_get()
    test_not_found()
    test_redirection()
    test_cgi_get()
    test_upload_post()
    print("\nPruebas completadas.")