import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from git import *
import make_assets

tag_re = re.compile(r'bootstrap-(?P<ver>\d+\.\d+.\d+)')

def get_inept_root():
    script_path = Path(os.path.abspath(__file__))
    return script_path.parents[3]


def get_app_root():
    script_path = Path(os.path.abspath(__file__))
    return script_path.parents[1]


def get_app_tags(repo):
    tags = sorted(list(filter(lambda x: tag_re.match(str(x)), repo.tags)), key=lambda t: t.tag.tagged_date)
    return tags


def get_prev_version(repo):
    tags = get_app_tags(repo)
    if tags:
        m = tag_re.match(str(tags[-1]))
        return m.group('ver')
    return '0.0.0'


def format_version(major, minor, patch):
    return f'{major}.{minor}.{patch}'


def get_next_version(repo):
    prev_version = get_prev_version(repo)
    major, minor, patch = prev_version.split('.')
    return format_version(major, minor, int(patch) + 1)


def release_package(release_version, arch, executable, assets, testing):
    release_dir = get_app_root() / 'releases' / release_version / arch
    release_dir.mkdir(parents=True, exist_ok=args.testing)
    shutil.copy2(Path(tempdir) / 'assets.zip', release_dir / 'assets.glp')
    package_exe_name = f'{executable.stem}_{arch}{executable.suffix}'
    shutil.copy2(executable, release_dir / package_exe_name)
    archive_name = f'releases/bootstrap-{arch}-{release_version}{"-testing" if testing else ""}'
    shutil.make_archive(get_app_root() / archive_name, 'zip', release_dir)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', dest='testing', action='store_true', help='Build release for testing')
    parser.add_argument('-v', dest='version', help='Version to build')
    args = parser.parse_args()

    inept_root = get_inept_root()
    repo = Repo(inept_root)

    if not args.testing and repo.is_dirty():
        raise RuntimeError("Repository is not up to date, cannot continue.")

    # Build ID
    head = repo.head.commit
    sha_head = head.hexsha
    build_id = repo.git.rev_parse(sha_head, short=7).upper()

    # Release tag
    release_version = get_next_version(repo) if not args.version else args.version
    release_version = f'{release_version}-testing' if args.testing else release_version
    release_tag = f'bootstrap-{release_version}'
    print(f"Building version '{release_version} using commit '{build_id}'")

    # Build
    msbuild =  r"C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\MSBuild\Current\Bin\MSBuild.exe"
    if not os.path.isfile(msbuild):
        raise RuntimeError("MsBuild not found.")

    solution = str(inept_root / 'project/inept.sln') # don't love this, should generate solutions for the apps
    print(f"Building {solution}...")
    target = '-t:bootstrap' if args.testing else '-t:bootstrap:Rebuild'
    configuration = '-p:Configuration=Release'
    platform = '-p:Platform=x64'
    appversion = f'-p:GliAppVersion=\\"Version {release_version} Build {build_id}\\"'
    shipping = '-p:GliShipping=1'
    subprocess.check_call([msbuild, solution, target, configuration, platform, appversion, shipping])

    platform = '-p:Platform=Win32'
    subprocess.check_call([msbuild, solution, target, configuration, platform, appversion, shipping])

    # Build assets
    with tempfile.TemporaryDirectory() as tempdir, open(get_app_root() / 'res/assets.txt') as asset_list:
        assets_dir = Path(tempdir) / 'assets'
        make_assets.process(asset_list, assets_dir)
        shutil.make_archive(Path(tempdir) / 'assets', 'zip', assets_dir)

        # Create release packages
        release_package(release_version, 'x86', get_inept_root() / 'project/_builds/bootstrap/Win32/Release/bin/bootstrap.exe', Path(tempdir) / 'assets.zip', args.testing)
        release_package(release_version, 'x64', get_inept_root() / 'project/_builds/bootstrap/Release/bin/bootstrap.exe', Path(tempdir) / 'assets.zip', args.testing)

    # Add tag to repo
    if not args.testing:
        new_tag = repo.create_tag(release_tag, ref=head, message=f'Bootstrap release {release_version}')
