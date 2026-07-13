import os
import shutil
from SCons.Script import Import

Import("env")

#
#   Fonction pour copier le firmware.bin apres une compilation reussie
#
def post_build_copy_firmware(source, target, env):
    print("--- Script post-compilation ---")
    
    # Chemin du firmware compile
    firmware_path = str(target[0])

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
    print(f"Firmware '{os.path.basename(firmware_path)}' copie et renomme en '{dest_file_name}' dans le dossier firmware.")
    print("-------------------------------")

env.AddPostAction("$PROGPATH", post_build_copy_firmware)