# $Id$
# Maintainer: Robert Goßmann <robert.gossmann.xyz>
# Contributor: Mauro Fruet <maurofruet@gmail.com>
# Contributor: William Di Luigi <williamdiluigi@gmail.com>

pkgname=evimce
pkgver=3.31.4+2+vim
pkgrel=1
pkgdesc="Simply a document viewer with vim key bindings"
url="http://projects.gnome.org/evince/"
arch=(i686 x86_64)
license=(GPL)
depends=(gtk3 libgxps libspectre gsfonts poppler-glib djvulibre t1lib libsecret
         desktop-file-utils dconf gsettings-desktop-schemas adwaita-icon-theme)
makedepends=(itstool libnautilus-extension texlive-bin gobject-introspection
             intltool docbook-xsl python gtk-doc git gnome-common)
optdepends=('texlive-bin: DVI support'
	          'gvfs: bookmark support and session saving')
provides=('evince')
conflicts=('evince')
options=('!emptydirs')
source=($pkgname::"git+https://github.com/elumbella/evince.git")
md5sums=('SKIP')


pkgver() {
  cd "$srcdir/$pkgname"
  git describe --tags | sed 's/-/+/g'
}

build() {
  cd $pkgname
  arch-meson build
  ninja -C build
}

package() {
  cd $pkgname
  DESTDIR="$pkgdir" meson install -C build
}

