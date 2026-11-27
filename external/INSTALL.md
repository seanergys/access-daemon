<!--
SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>

SPDX-License-Identifier: Apache-2.0
-->

## EAR

```
git submodule update --remote external/ear
git add external/ear
git commit -m "Update external/ear submodule"
git push
```

## SeanerBUS C++ Bindings

```
git clone git@gitlab.seanergys.fz-juelich.de:software/seanerbus.git
cd seanerbus

git pull
git submodule sync --recursive
git submodule update --init --recursive

cmake -S bindings/cpp -B bindings/cpp/build -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=/home/jordi/Documents/opt/seanerbus -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
cmake --build bindings/cpp/build --parallel
cmake --install bindings/cpp/build
```