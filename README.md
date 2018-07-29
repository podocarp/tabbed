# About
This is pretty much the original tabbed utility but patched and fixed because some patches are a bit old.

# Patches

- alpha
  - adds transparency support
  - the orginal patch did not account for an update in `config.mk`.
  - the original patch direction for st is wrong.
- autohide
  - hides tab bar when there is only one tab.
- clientnumber
  - shows the tab number because you're definitely not going to count them yourself.
- xft
  - adds UTF8 support.

# alpha
To get st working with alpha, do the change noted on the page but at `x.c` instead of `st.c`.

Quoting the page:
If you want to use transparency in st with this patch, you also need to replace

```C
define USE_ARGB (alpha != OPAQUE && opt_embed == NULL)

```

by

```C
define USE_ARGB (alpha != OPAQUE)

```

# Usage

`sudo make install`.

Try `man tabbed`.
tl;dr: you can try `tabbed st -w` `tabbed -r 2 st -w '' -n "helloworld"` and stuff like that.

tabbed - generic tabbed interface
=================================
tabbed is a simple tabbed X window container.

Requirements
------------
In order to build tabbed you need the Xlib header files.

Installation
------------
Edit config.mk to match your local setup (tabbed is installed into
the /usr/local namespace by default).

Afterwards enter the following command to build and install tabbed
(if necessary as root):

    make clean install

Running tabbed
--------------
See the man page for details.

