# Cuteataxx
Cuteataxx runs computer engine matches for the board game [Ataxx](https://en.wikipedia.org/wiki/Ataxx). Still a work in progress.

---

# Usage
Match settings are read from a .json file passed to cuteataxx through command line arguments
```
./cuteataxx settings.json
```

---

# Building
```
git clone --recurse-submodules https://github.com/kz04px/cuteataxx
cd ./cuteataxx
sh build-linux.sh
```
or
```
git clone --recurse-submodules https://github.com/kz04px/cuteataxx
follow libataxx build instructions
mkdir ./cuteataxx/build
cd ./cuteataxx/build
cmake ..
make
```

---

# Settings
Match settings are provided in the [JSON](https://en.wikipedia.org/wiki/JSON) file format. An example of which can be found in the `res` directory [here](./res/settings.json). Details of the settings available can be found [here](./settings.md).

---

# License
Cuteataxx is available under the MIT license.

---

# Thanks
- [JSON for Modern C++](https://github.com/nlohmann/json) JSON for Modern C++
- To everyone partaking in Ataxx engine programming
