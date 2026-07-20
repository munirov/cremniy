%global qt_ver 6

Name:           cremniy
Version:        ${TAG_VERSION}
Release:        10%{?dist}

Summary:        IDE for low level developer
Summary(ru):    Редактор кода для низкоуровневой разработки

License:        GPL-3.0
URL:            https://github.com/munirov/cremniy

Source0:        %{name}-%{version}.tar.gz


BuildRequires: cmake
BuildRequires: gcc-c++
BuildRequires: qt%{qt_ver}-qtbase-devel
BuildRequires: qt%{qt_ver}-qttools-devel
BuildRequires: qt%{qt_ver}-qtsvg-devel
BuildRequires: desktop-file-utils


Requires: qt%{qt_ver}-qtbase
Requires: qt%{qt_ver}-qttools
Requires: qt%{qt_ver}-qtsvg


%description
IDE for low level developer


%description -l ru
Редактор кода для низкоуровневой разработки


%prep
%autosetup -n %{name}


%build

rm -rf build

%cmake -S src/ \
    -DCMAKE_BUILD_TYPE=Release

%cmake_build

%install

%{__rm} -rf %{buildroot}

%cmake_install

install -Dm644 \
    docs/cremniy_icon_stroke.svg \
    %{buildroot}%{_datadir}/icons/hicolor/scalable/apps/cremniy.svg

desktop-file-install \
    --dir=%{buildroot}%{_datadir}/applications \
    rpm/%{name}.desktop

install -d %{buildroot}%{_bindir}/Resources/translations

%{__cp} %{__cmake_builddir}/Resources/translations/*.qm \
   %{buildroot}%{_bindir}/Resources/translations/

%post
%{_bindir}/update-desktop-database %{_datadir}/applications >/dev/null 2>&1 || :
%icons_scriptlet

%postun
%{_bindir}/update-desktop-database %{_datadir}/applications >/dev/null 2>&1 || :
%icons_scriptlet

%files

%license LICENSE
%doc README.md

%{_bindir}/cremniy

%{_datadir}/applications/%{name}.desktop

%{_datadir}/icons/hicolor/scalable/apps/cremniy.svg

%{_bindir}/Resources/translations/*.qm

%changelog

* Tue Jul 14 2026 Dmitriy <faketriplus@yandex.ru>
- Add translations

* Mon Jul 13 2026 Dmitriy <faketriplus@yandex.ru>
- Initial package
