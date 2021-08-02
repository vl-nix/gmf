### [Gmf](https://github.com/vl-nix/gmf)

* File Manager
* Shortcuts: Ctrl + X, Ctrl + C, Ctrl + V, Ctrl + H, Delete


#### Dependencies

* gcc, meson
* libgtk 3.0 ( & dev )
* File-roller or Engrampa


#### Build

1. Configure: meson build --prefix /usr --strip

2. Build: ninja -C build

3. Install: sudo ninja -C build install

4. Uninstall: sudo ninja -C build uninstall

5. Debug: GMF_DEBUG=1 gmf
