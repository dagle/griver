giver - glib bindings to river
==============================

[river](https://github.com/riverwm/river) is a dynamic tiling Wayland
compositor. River exposes a protocol called river-layout which lets you
implement different tiling algorithms.

What giver does it that it exposes a this protocol into a glib gobject, using a
signal to handle different callbacks. This allows us get a lot of language
bindings for free. See
[implentations](https://gi.readthedocs.io/en/latest/users.html) for some of
them.

How to use giver for your language depends on the language. Checking out
examples for your languages gir bindings on how should be enough to use this.
Especially how to bind signals. In the future there will be some basic
examples.
