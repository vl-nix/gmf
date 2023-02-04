### [Gmf](https://github.com/vl-nix/gmf)

* File Manager
* Copy & Paste: Drag and Drop


#### Dependencies

* gcc
* meson
* libgtk 3.0 ( & dev )
* File-roller or Engrampa


#### Build

1. Clone: git clone https://github.com/vl-nix/gmf.git

2. Configure: meson build --prefix /usr --strip

3. Build: ninja -C build

4. Install: ninja install -C build

5. Uninstall: ninja uninstall -C build
