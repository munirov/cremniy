pkgname=cremniy-git
_pkgname=cremniy
pkgver=0.1.r264.g8d56f78 
pkgrel=1
pkgdesc="Cremniy is an integrated environment for low-level development."
arch=('x86_64')
url="https://github.com/munirov/cremniy"
license=('GPL3')
depends=('qt6-base')
makedepends=('cmake')
source=("src_repo::git+https://github.com/munirov/cremniy.git")
md5sums=('SKIP')

pkgver() {
  cd "$srcdir/src_repo"
  printf "0.1.r%s.g%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cmake -S "$srcdir/src_repo/src" -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
  cmake --build build
}

package() {
  DESTDIR="$pkgdir" cmake --install build
}
