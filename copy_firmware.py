import os
import shutil
import hashlib
import json
import re
from SCons.Script import Import

Import("env")

def get_firmware_md5(firmware_path):
    """Calcule l'empreinte MD5 d'un fichier."""
    md5 = hashlib.md5()
    with open(firmware_path, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            md5.update(chunk)
    return md5.hexdigest()

def get_firmware_version(project_dir):
    """Extrait la version du firmware depuis src/shared.h."""
    shared_h_path = os.path.join(project_dir, "src", "shared.h")
    try:
        with open(shared_h_path, "r") as f:
            content = f.read()
            match = re.search(r'#define\s+FIRMWARE_VERSION\s+(\d+)', content)
            if match:
                version = int(match.group(1))
                print(f"Version du firmware trouvee dans shared.h : {version}")
                return version
    except Exception as e:
        print(f"AVERTISSEMENT: Impossible de lire la version depuis shared.h: {e}")
    return None

#
#   Fonction pour copier le firmware.bin apres une compilation reussie
#
def post_build_copy_firmware(source, target, env):
    print("--- Script post-compilation ---")
    
    # Le 'target' est le fichier .bin qui vient d'etre cree
    firmware_path = str(target[0])

    # Calcule l'empreinte MD5 du firmware
    firmware_md5 = get_firmware_md5(firmware_path)
    print(f"MD5 du firmware : {firmware_md5}")

    # Extrait la version du firmware depuis shared.h
    firmware_version = get_firmware_version(env.get("PROJECT_DIR"))

    # Dossier de destination
    dest_dir = os.path.join(env.get("PROJECT_DIR"), "firmware")

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
    version_json_path = os.path.join(env.get("PROJECT_DIR"), "firmware", "version.json")
    try:
        data = {}
        # Essayer de lire le fichier existant pour conserver d'autres donnees potentielles
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

    print("-------------------------------")

# On accroche le script a la creation du fichier .bin final, et non plus au .elf
# On construit le chemin avec une simple chaine de caracteres, SCons s'occupe du reste.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", post_build_copy_firmware)