import os
import shutil
import hashlib
import json
import re
import ssl
import certifi
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from SCons.Script import Import

Import("env")

# --- CONFIGURATION ---
GITHUB_USER_REPO = "djfab59/PixelCross"
TOKEN_FILE_NAME = ".github_token"

# Contexte SSL pour les requetes HTTPS (fix macOS / Python embarque)
SSL_CONTEXT = ssl.create_default_context(cafile=certifi.where())

def get_firmware_md5(firmware_path):
    """Calcule l'empreinte MD5 d'un fichier."""
    md5 = hashlib.md5()
    with open(firmware_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            md5.update(chunk)
    return md5.hexdigest()

def get_firmware_version(project_dir):
    """Extrait la version du firmware depuis src/shared.h (format string 'X.Y')."""
    shared_h_path = os.path.join(project_dir, "src", "shared.h")
    try:
        with open(shared_h_path, "r") as f:
            content = f.read()
            match = re.search(r'#define\s+FIRMWARE_VERSION\s+"([^"]+)"', content)
            if match:
                version = match.group(1)
                print(f"Version du firmware trouvee dans shared.h : {version}")
                return version
    except Exception as e:
        print(f"AVERTISSEMENT: Impossible de lire la version depuis shared.h: {e}")
    return None

def get_github_token(project_dir):
    """Lit le token GitHub depuis le fichier local .github_token."""
    token_path = os.path.join(project_dir, TOKEN_FILE_NAME)
    try:
        with open(token_path, "r") as f:
            token = f.read().strip()
            if token:
                return token
    except FileNotFoundError:
        pass
    return None

def github_api_request(url, token, method="GET", data=None, content_type="application/json"):
    """Effectue une requete vers l'API GitHub."""
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github.v3+json",
        "User-Agent": "PixelCross-PostBuild"
    }
    if content_type:
        headers["Content-Type"] = content_type

    body = None
    if data is not None:
        if isinstance(data, bytes):
            body = data
        else:
            body = json.dumps(data).encode("utf-8")

    req = Request(url, data=body, headers=headers, method=method)
    response = urlopen(req, context=SSL_CONTEXT)
    return json.loads(response.read().decode("utf-8"))

def upload_release_asset(upload_url, token, file_path, content_type="application/octet-stream"):
    """Upload un fichier en tant qu'asset sur une release GitHub."""
    file_name = os.path.basename(file_path)
    # L'URL d'upload contient {?name,label} qu'il faut remplacer
    url = upload_url.replace("{?name,label}", f"?name={file_name}")

    with open(file_path, "rb") as f:
        file_data = f.read()

    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github.v3+json",
        "User-Agent": "PixelCross-PostBuild",
        "Content-Type": content_type,
        "Content-Length": str(len(file_data))
    }

    req = Request(url, data=file_data, headers=headers, method="POST")
    response = urlopen(req, context=SSL_CONTEXT)
    return json.loads(response.read().decode("utf-8"))

def create_github_release(project_dir, firmware_version, version_json_path, firmware_zip_path):
    """Cree une release GitHub et y attache les fichiers firmware."""
    token = get_github_token(project_dir)
    if not token:
        print(f"  Pas de fichier '{TOKEN_FILE_NAME}' trouve. Release GitHub ignoree.")
        print(f"  Pour activer la publication automatique, creez un fichier '{TOKEN_FILE_NAME}'")
        print(f"  a la racine du projet contenant votre Personal Access Token GitHub.")
        return False

    tag_name = f"v{firmware_version}"
    release_name = f"Firmware v{firmware_version}"

    print(f"  Creation de la release GitHub '{release_name}' (tag: {tag_name})...")

    try:
        # 1. Creer la release
        release_data = {
            "tag_name": tag_name,
            "name": release_name,
            "body": f"Release automatique du firmware v{firmware_version}",
            "draft": False,
            "prerelease": False
        }
        api_url = f"https://api.github.com/repos/{GITHUB_USER_REPO}/releases"
        release = github_api_request(api_url, token, method="POST", data=release_data)
        upload_url = release["upload_url"]
        print(f"  Release creee avec succes (id: {release['id']})")

        # 2. Upload version.json
        print(f"  Upload de version.json...")
        upload_release_asset(upload_url, token, version_json_path, "application/json")
        print(f"  version.json uploade.")

        # 3. Upload firmware .zip
        print(f"  Upload de {os.path.basename(firmware_zip_path)}...")
        upload_release_asset(upload_url, token, firmware_zip_path, "application/octet-stream")
        print(f"  Firmware uploade.")

        print(f"  Release v{firmware_version} publiee avec succes !")
        return True

    except HTTPError as e:
        error_body = e.read().decode("utf-8") if e.fp else "Pas de details"
        print(f"  ERREUR lors de la creation de la release GitHub: HTTP {e.code}")
        print(f"  Details: {error_body}")
        return False
    except Exception as e:
        print(f"  ERREUR lors de la creation de la release GitHub: {e}")
        return False

#
#   Fonction principale post-compilation
#
def post_build_copy_firmware(source, target, env):
    print("--- Script post-compilation ---")
    
    # Le 'target' est le fichier .bin qui vient d'etre cree
    firmware_path = str(target[0])
    project_dir = env.get("PROJECT_DIR")

    # Extrait la version du firmware depuis shared.h
    firmware_version = get_firmware_version(project_dir)

    # Verifie si la version a change par rapport a version.json
    # Si la version est identique, on ne regenere pas les fichiers de release (evite le bruit)
    version_json_path = os.path.join(project_dir, "firmware", "version.json")
    if firmware_version is not None and os.path.exists(version_json_path):
        try:
            with open(version_json_path, "r") as f:
                existing_data = json.load(f)
            if existing_data.get("version") == firmware_version:
                print(f"Version inchangee ({firmware_version}). Pas de mise a jour des fichiers de release.")
                print("-------------------------------")
                return
        except Exception:
            pass  # En cas d'erreur de lecture, on continue normalement

    # Calcule l'empreinte MD5 du firmware
    firmware_md5 = get_firmware_md5(firmware_path)
    print(f"MD5 du firmware : {firmware_md5}")

    # Dossier de destination
    dest_dir = os.path.join(project_dir, "firmware")

    # S'assurer que le dossier de destination existe
    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    # Nom du fichier de destination, incluant la plateforme pour le rendre unique
    env_name = env['PIOENV']
    dest_file_name = f"firmware-{env_name}.zip"
    dest_path = os.path.join(dest_dir, dest_file_name)

    # Copier et renommer le fichier
    shutil.copy(firmware_path, dest_path)
    print(f"Firmware '{os.path.basename(firmware_path)}' copie et renomme en '{dest_file_name}'.")

    # Met a jour le fichier version.json avec la nouvelle empreinte MD5 et la version
    try:
        data = {}
        if os.path.exists(version_json_path):
            with open(version_json_path, "r") as f:
                data = json.load(f)
        
        # Mettre a jour les valeurs
        if firmware_version is not None:
            data['version'] = firmware_version
        data['md5'] = firmware_md5

        # Reecrire le fichier
        with open(version_json_path, "w") as f:
            json.dump(data, f, indent=2)
            
        print(f"version.json mis a jour (version: {data.get('version', 'N/A')}, md5: {data.get('md5', 'N/A')})")
    except Exception as e:
        print(f"AVERTISSEMENT: Impossible de mettre a jour le MD5 dans version.json: {e}")

    # --- PUBLICATION AUTOMATIQUE SUR GITHUB ---
    print("--- Publication GitHub ---")
    create_github_release(project_dir, firmware_version, version_json_path, dest_path)

    print("-------------------------------")

# On accroche le script a la creation du fichier .bin final, et non plus au .elf
# On construit le chemin avec une simple chaine de caracteres, SCons s'occupe du reste.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_build_copy_firmware)
