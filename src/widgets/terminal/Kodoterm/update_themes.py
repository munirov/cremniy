#!/usr/bin/env python3
import os
import shutil
import subprocess
import glob
import re

def run_command(command, cwd=None):
    try:
        subprocess.check_call(command, shell=True, cwd=cwd)
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {command}")
        print(e)
        exit(1)

def normalize_name(filename):
    # Remove extension
    name = os.path.splitext(os.path.basename(filename))[0]
    # Lowercase and remove non-alphanumeric characters
    return re.sub(r'[^a-z0-9]', '', name.lower())

def main():
    base_dir = os.path.dirname(os.path.abspath(__file__))
    themes_dir = os.path.join(base_dir, "KodoTermThemes")
    tmp_dir = os.path.join(base_dir, "tmp_themes_download")

    # Clean up temp dir if exists
    if os.path.exists(tmp_dir):
        shutil.rmtree(tmp_dir)
    os.makedirs(tmp_dir)

    print("--- Downloading Themes ---")

    # 1. Konsole Themes (from KDE)
    print("Fetching Konsole themes...")
    konsole_repo_dir = os.path.join(tmp_dir, "konsole")
    run_command(f"git clone --depth 1 https://github.com/KDE/konsole.git {konsole_repo_dir}")
    
    dest_konsole = os.path.join(themes_dir, "konsole")
    if os.path.exists(dest_konsole):
        shutil.rmtree(dest_konsole)
    os.makedirs(dest_konsole)
    
    src_konsole_files = glob.glob(os.path.join(konsole_repo_dir, "data", "color-schemes", "*.colorscheme"))
    known_konsole_map = {} # norm_name -> original_name
    for f in src_konsole_files:
        shutil.copy(f, dest_konsole)
        known_konsole_map[normalize_name(f)] = os.path.basename(f)
    konsole_count = len(src_konsole_files)

    # 2. iTerm2 Themes and Windows Terminal Themes (from mbadolato/iTerm2-Color-Schemes)
    print("Fetching iTerm2 and Windows Terminal themes...")
    iterm_repo_dir = os.path.join(tmp_dir, "iterm2")
    run_command(f"git clone --depth 1 https://github.com/mbadolato/iTerm2-Color-Schemes.git {iterm_repo_dir}")

    # Windows Terminal Themes
    dest_wt = os.path.join(themes_dir, "windowsterminal")
    if os.path.exists(dest_wt):
        shutil.rmtree(dest_wt)
    os.makedirs(dest_wt)

    src_wt_files = glob.glob(os.path.join(iterm_repo_dir, "windowsterminal", "*.json"))
    known_wt_map = {} # norm_name -> original_name
    for f in src_wt_files:
        shutil.copy(f, dest_wt)
        known_wt_map[normalize_name(f)] = os.path.basename(f)
    wt_count = len(src_wt_files)

    # iTerm Themes
    dest_iterm = os.path.join(themes_dir, "iterm")
    if os.path.exists(dest_iterm):
        shutil.rmtree(dest_iterm)
    os.makedirs(dest_iterm)
    
    src_iterm_files = glob.glob(os.path.join(iterm_repo_dir, "schemes", "*.itermcolors"))
    iterm_count = 0
    duplicates_konsole = []
    duplicates_wt = []
    
    print("Processing iTerm themes (filtering duplicates)...")
    for f in src_iterm_files:
        norm_name = normalize_name(f)
        base = os.path.basename(f)
        
        if norm_name in known_konsole_map:
            duplicates_konsole.append(f"{base} (matches Konsole: {known_konsole_map[norm_name]})")
            continue
            
        if norm_name in known_wt_map:
            duplicates_wt.append(f"{base} (matches Windows Terminal: {known_wt_map[norm_name]})")
            continue
            
        shutil.copy(f, dest_iterm)
        iterm_count += 1
        
    # 3. Generate KodoTermThemes.qrc
    print("Regenerating KodoTermThemes.qrc...")
    qrc_path = os.path.join(base_dir, "KodoTermThemes.qrc")
    
    with open(qrc_path, "w") as f:
        f.write('<!DOCTYPE RCC><RCC version="1.0">\n')
        f.write('<qresource prefix="/">\n')
        
        # Konsole
        for entry in sorted(os.listdir(dest_konsole)):
            if entry.endswith(".colorscheme"):
                f.write(f'    <file>KodoTermThemes/konsole/{entry}</file>\n')

        # Windows Terminal
        for entry in sorted(os.listdir(dest_wt)):
            if entry.endswith(".json"):
                f.write(f'    <file>KodoTermThemes/windowsterminal/{entry}</file>\n')

        # iTerm
        for entry in sorted(os.listdir(dest_iterm)):
            if entry.endswith(".itermcolors"):
                f.write(f'    <file>KodoTermThemes/iterm/{entry}</file>\n')

        f.write('</qresource>\n')
        f.write('</RCC>\n')

    print("\n--- Theme Statistics ---")
    print(f"Konsole (KDE):          {konsole_count}")
    print(f"Windows Terminal (iT):  {wt_count}")
    print(f"iTerm (Unique):         {iterm_count}")
    print(f"Total Unique Themes:    {konsole_count + wt_count + iterm_count}")
    
    print(f"\nDuplicates skipped in iTerm folder: {len(duplicates_konsole) + len(duplicates_wt)}")
    if duplicates_konsole:
        print(f"  Matches in Konsole ({len(duplicates_konsole)}):")
        for d in sorted(duplicates_konsole)[:10]: # Show first 10
            print(f"    - {d}")
        if len(duplicates_konsole) > 10:
            print(f"    ... and {len(duplicates_konsole)-10} more")

    if duplicates_wt:
        print(f"\n  Matches in Windows Terminal ({len(duplicates_wt)}):")
        for d in sorted(duplicates_wt)[:10]: # Show first 10
            print(f"    - {d}")
        if len(duplicates_wt) > 10:
            print(f"    ... and {len(duplicates_wt)-10} more")

    print("\nCleaning up...")
    shutil.rmtree(tmp_dir)
    print("Done.")

if __name__ == "__main__":
    main()