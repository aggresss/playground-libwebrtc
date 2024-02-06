# playground-libwebrtc


## 0x01 environment

### macOS

Step 01 prerequisite

```shell
# ninja
brew install ninja

# depot_tools
cd ${HOME}/workspace-formal
git_clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
env_append PATH ${HOME}/workspace-formal/chromium.googlesource.com/tools/depot_tools
env_amend DEPOT_TOOLS_UPDATE 0

# gn
cd ${HOME}/workspace-formal
git_clone https://gn.googlesource.com/gn.git
cd ${HOME}/workspace-formal/gn.googlesource.com/gn
python build/gen.py
ninja -C out
env_append PATH ${HOME}/workspace-formal/gn.googlesource.com/gn/out

```

Step 02 build

```shell
mkdir_cd ${HOME}/workspace-scratch/aggresss/playground-libwebrtc

cat << END > ./.gclient
solutions = [
  {
    "url": "git@github.com:aggresss/playground-libwebrtc.git@playground-m121",
    "managed": False,
    "name": "src",
    "deps_file": "DEPS",
    "custom_deps": {},
  },
]
END

gclient sync --verbose
cd src
gn gen out/Default
ninja -C out/Default
```
