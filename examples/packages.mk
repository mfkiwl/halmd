#
# Copyright © 2011  Peter Colberg
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# default installation prefix
PREFIX = $(HOME)/opt

# support parallel builds
ifdef CONCURRENCY_LEVEL
    PARALLEL_BUILD_FLAGS = -j$(CONCURRENCY_LEVEL)
endif

##
## define commonly used commands
##

WGET = wget
TAR = tar
UNZIP = unzip
RM = rm -rf
CP = cp -r
TOUCH = touch
PATCH = patch
SHA256SUM = sha256sum --check

##
## define top-level targets
##

build: build-cmake build-lua build-boost build-luabind build-hdf5

fetch: fetch-cmake fetch-lua fetch-boost fetch-luabind fetch-hdf5

install: install-cmake install-lua install-boost install-luabind install-hdf5

clean: clean-cmake clean-lua clean-boost clean-luabind clean-hdf5

distclean: distclean-cmake distclean-lua distclean-boost distclean-luabind distclean-hdf5

env: env-cmake env-lua env-boost env-luabind env-hdf5

##
## CMake with CMake-CUDA patch
##

CMAKE_VERSION = 2.8.7
CMAKE_TARBALL = cmake-$(CMAKE_VERSION).tar.gz
CMAKE_TARBALL_URL = http://www.cmake.org/files/v2.8/$(CMAKE_TARBALL)
CMAKE_TARBALL_SHA256 = 130923053d8fe1a2ae032a3f09021f9024bf29d7a04ed10ae04647ff00ecf59f
CMAKE_CUDA_PATCH = cmake-cuda-2.8.7-0-ga9ce0fa.patch
CMAKE_CUDA_PATCH_URL = http://sourceforge.net/projects/halmd/files/patches/$(CMAKE_CUDA_PATCH)
CMAKE_CUDA_PATCH_SHA256 = f54bd884eebb81190e102f0ed5736af92d42301abd21039cf61ce8c302c7fabe
CMAKE_BUILD_DIR = cmake-$(CMAKE_VERSION)
CMAKE_INSTALL_DIR = $(PREFIX)/cmake-$(CMAKE_VERSION)

.fetch-cmake:
	@$(RM) $(CMAKE_TARBALL)
	@$(RM) $(CMAKE_CUDA_PATCH)
	$(WGET) $(CMAKE_TARBALL_URL)
	$(WGET) $(CMAKE_CUDA_PATCH_URL)
	@echo '$(CMAKE_TARBALL_SHA256)  $(CMAKE_TARBALL)' | $(SHA256SUM)
	@echo '$(CMAKE_CUDA_PATCH_SHA256)  $(CMAKE_CUDA_PATCH)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-cmake: .fetch-cmake

.extract-cmake: .fetch-cmake
	$(RM) $(CMAKE_BUILD_DIR)
	$(TAR) -xzf $(CMAKE_TARBALL)
	cd $(CMAKE_BUILD_DIR) && $(PATCH) -p1 < $(CURDIR)/$(CMAKE_CUDA_PATCH)
	@$(TOUCH) $@

extract-cmake: .extract-cmake

.configure-cmake: .extract-cmake
	cd $(CMAKE_BUILD_DIR) && ./configure --prefix=$(CMAKE_INSTALL_DIR)
	@$(TOUCH) $@

configure-cmake: .configure-cmake

.build-cmake: .configure-cmake
	cd $(CMAKE_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-cmake: .build-cmake

install-cmake: .build-cmake
	cd $(CMAKE_BUILD_DIR) && make install

clean-cmake:
	@$(RM) .build-cmake
	@$(RM) .configure-cmake
	@$(RM) .extract-cmake
	$(RM) $(CMAKE_BUILD_DIR)

distclean-cmake: clean-cmake
	@$(RM) .fetch-cmake
	$(RM) $(CMAKE_TARBALL)
	$(RM) $(CMAKE_CUDA_PATCH)

env-cmake:
	@echo
	@echo '# add CMake $(CMAKE_VERSION) to environment'
	@echo 'export PATH="$(CMAKE_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(CMAKE_INSTALL_DIR)/man$${MANPATH+:$$MANPATH}"'

##
## Lua
##

LUA_VERSION = 5.2.0
LUA_TARBALL = lua-$(LUA_VERSION).tar.gz
LUA_TARBALL_URL = http://www.lua.org/ftp/$(LUA_TARBALL)
LUA_TARBALL_SHA256 = cabe379465aa8e388988073d59b69e76ba0025429d2c1da80821a252cdf6be0d
LUA_BUILD_DIR = lua-$(LUA_VERSION)
LUA_INSTALL_DIR = $(PREFIX)/lua-$(LUA_VERSION)
LUA_CFLAGS = -DLUA_USE_LINUX -DLUA_COMPAT_MODULE -fPIC -O2 -Wall

.fetch-lua:
	@$(RM) $(LUA_TARBALL)
	$(WGET) $(LUA_TARBALL_URL)
	@echo '$(LUA_TARBALL_SHA256)  $(LUA_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-lua: .fetch-lua

.extract-lua: .fetch-lua
	$(RM) $(LUA_BUILD_DIR)
	$(TAR) -xzf $(LUA_TARBALL)
	@$(TOUCH) $@

extract-lua: .extract-lua

.build-lua: .extract-lua
	cd $(LUA_BUILD_DIR) && make linux CFLAGS="$(LUA_CFLAGS)"
	@$(TOUCH) $@

build-lua: .build-lua

install-lua: .build-lua
	cd $(LUA_BUILD_DIR) && make install INSTALL_TOP=$(LUA_INSTALL_DIR)

clean-lua:
	@$(RM) .build-lua
	@$(RM) .extract-lua
	$(RM) $(LUA_BUILD_DIR)

distclean-lua: clean-lua
	@$(RM) .fetch-lua
	$(RM) $(LUA_TARBALL)

env-lua:
	@echo
	@echo '# add Lua $(LUA_VERSION) to environment'
	@echo 'export PATH="$(LUA_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(LUA_INSTALL_DIR)/man$${MANPATH+:$$MANPATH}"'
	@echo 'export CMAKE_PREFIX_PATH="$(LUA_INSTALL_DIR)$${CMAKE_PREFIX_PATH+:$$CMAKE_PREFIX_PATH}"'

##
## Boost C++ libraries with Boost.Log
##

BOOST_VERSION = 1.48.0
BOOST_RELEASE = 1_48_0
BOOST_TARBALL = boost_$(BOOST_RELEASE).tar.gz
BOOST_TARBALL_URL = http://sourceforge.net/projects/boost/files/boost/$(BOOST_VERSION)/$(BOOST_TARBALL)
BOOST_TARBALL_SHA256 = 01c8c3330a7a5013b8cfab18a3b80fcfd89001701ea5907c9ae635b97bc2c789
BOOST_LOG_VERSION = 1.1
BOOST_LOG_TARBALL = boost-log-$(BOOST_LOG_VERSION).zip
BOOST_LOG_TARBALL_URL = http://sourceforge.net/projects/boost-log/files/boost-log-$(BOOST_LOG_VERSION).zip
BOOST_LOG_TARBALL_SHA256 = 4b00e1d302017298284914c6cc9e7fcae0e097c93e632045d6b0fc4bf6266ba7
BOOST_LOG_DIR = boost-log-$(BOOST_LOG_VERSION)
BOOST_BUILD_DIR = boost_$(BOOST_RELEASE)
BOOST_INSTALL_DIR = $(PREFIX)/boost_$(BOOST_RELEASE)
BOOST_BUILD_FLAGS = cxxflags=-fPIC dll-path=$(BOOST_INSTALL_DIR)/lib --without-python

.fetch-boost:
	@$(RM) $(BOOST_TARBALL)
	@$(RM) $(BOOST_LOG_TARBALL)
	$(WGET) $(BOOST_TARBALL_URL)
	$(WGET) -O $(BOOST_LOG_TARBALL) $(BOOST_LOG_TARBALL_URL)
	@echo '$(BOOST_TARBALL_SHA256)  $(BOOST_TARBALL)' | $(SHA256SUM)
	@echo '$(BOOST_LOG_TARBALL_SHA256)  $(BOOST_LOG_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-boost: .fetch-boost

.extract-boost: .fetch-boost
	$(RM) $(BOOST_BUILD_DIR) $(BOOST_LOG_DIR)
	$(TAR) -xzf $(BOOST_TARBALL)
	$(UNZIP) $(BOOST_LOG_TARBALL)
	$(CP) $(BOOST_LOG_DIR)/boost/log $(BOOST_BUILD_DIR)/boost/
	$(CP) $(BOOST_LOG_DIR)/libs/log $(BOOST_BUILD_DIR)/libs/
	@$(TOUCH) $@

extract-boost: .extract-boost

.configure-boost: .extract-boost
	cd $(BOOST_BUILD_DIR) && ./bootstrap.sh
	@$(TOUCH) $@

configure-boost: .configure-boost

.build-boost: .configure-boost
	cd $(BOOST_BUILD_DIR) && ./bjam $(BOOST_BUILD_FLAGS) $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-boost: .build-boost

install-boost: .build-boost
	cd $(BOOST_BUILD_DIR) && ./bjam $(BOOST_BUILD_FLAGS) install --prefix=$(BOOST_INSTALL_DIR)

clean-boost:
	@$(RM) .build-boost
	@$(RM) .configure-boost
	@$(RM) .extract-boost
	$(RM) $(BOOST_BUILD_DIR) $(BOOST_LOG_DIR)

distclean-boost: clean-boost
	@$(RM) .fetch-boost
	$(RM) $(BOOST_TARBALL)
	$(RM) $(BOOST_LOG_TARBALL)

env-boost:
	@echo
	@echo '# add Boost $(BOOST_VERSION) to environment'
	@echo 'export PATH="$(BOOST_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export LD_LIBRARY_PATH="$(BOOST_INSTALL_DIR)/lib$${LD_LIBRARY_PATH+:$$LD_LIBRARY_PATH}"'
	@echo 'export PYTHONPATH="$(BOOST_INSTALL_DIR)/lib$${PYTHONPATH+:$$PYTHONPATH}"'
	@echo 'export CMAKE_PREFIX_PATH="$(BOOST_INSTALL_DIR)$${CMAKE_PREFIX_PATH+:$$CMAKE_PREFIX_PATH}"'

##
## Luabind library
##

LUABIND_VERSION = 0.9.1-24-gf5fe839
LUABIND_TARBALL = luabind-$(LUABIND_VERSION).tar.bz2
LUABIND_TARBALL_URL = http://sourceforge.net/projects/halmd/files/libs/luabind/$(LUABIND_TARBALL)
LUABIND_TARBALL_SHA256 = a2e8cf5d876a6df824b9b98680068d6c37c16ef543f782b2fd0a45467edc1432
LUABIND_BUILD_DIR = luabind-$(LUABIND_VERSION)
LUABIND_BUILD_FLAGS = cxxflags=-fPIC link=static variant=release variant=debug
LUABIND_INSTALL_DIR = $(PREFIX)/luabind-$(LUABIND_VERSION)

.fetch-luabind:
	@$(RM) $(LUABIND_TARBALL)
	$(WGET) $(LUABIND_TARBALL_URL)
	@echo '$(LUABIND_TARBALL_SHA256)  $(LUABIND_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-luabind: .fetch-luabind

.extract-luabind: .fetch-luabind .build-lua
	$(RM) $(LUABIND_BUILD_DIR)
	$(TAR) -xjf $(LUABIND_TARBALL)
	mkdir $(LUABIND_BUILD_DIR)/lua
	ln -s $(CURDIR)/$(LUA_BUILD_DIR)/src $(LUABIND_BUILD_DIR)/lua/include
	ln -s $(CURDIR)/$(LUA_BUILD_DIR)/src $(LUABIND_BUILD_DIR)/lua/lib
	@$(TOUCH) $@

extract-luabind: .extract-luabind

.build-luabind: .extract-luabind .configure-boost
	cd $(LUABIND_BUILD_DIR) && BOOST_ROOT=$(CURDIR)/$(BOOST_BUILD_DIR) LUA_PATH=$(CURDIR)/$(LUABIND_BUILD_DIR)/lua $(CURDIR)/$(BOOST_BUILD_DIR)/bjam $(LUABIND_BUILD_FLAGS) $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-luabind: .build-luabind

install-luabind: .build-luabind
	cd $(LUABIND_BUILD_DIR) && BOOST_ROOT=$(CURDIR)/$(BOOST_BUILD_DIR) LUA_PATH=$(CURDIR)/$(LUABIND_BUILD_DIR)/lua $(CURDIR)/$(BOOST_BUILD_DIR)/bjam $(LUABIND_BUILD_FLAGS) install --prefix=$(LUABIND_INSTALL_DIR)

clean-luabind:
	@$(RM) .build-luabind
	@$(RM) .extract-luabind
	$(RM) $(LUABIND_BUILD_DIR)

distclean-luabind: clean-luabind
	@$(RM) .fetch-luabind
	$(RM) $(LUABIND_TARBALL)

env-luabind:
	@echo
	@echo '# add Luabind $(LUABIND_VERSION) to environment'
	@echo 'export LD_LIBRARY_PATH="$(LUABIND_INSTALL_DIR)/lib$${LD_LIBRARY_PATH+:$$LD_LIBRARY_PATH}"'
	@echo 'export CMAKE_PREFIX_PATH="$(LUABIND_INSTALL_DIR)$${CMAKE_PREFIX_PATH+:$$CMAKE_PREFIX_PATH}"'

##
## HDF5 C++ library
##

HDF5_VERSION = 1.8.8
HDF5_TARBALL = hdf5-$(HDF5_VERSION).tar.bz2
HDF5_TARBALL_URL = http://www.hdfgroup.org/ftp/HDF5/releases/hdf5-$(HDF5_VERSION)/src/$(HDF5_TARBALL)
HDF5_TARBALL_SHA256 = b0ebb0b5478c6c0427631d4ad08f96e39f1b09fde615aa98d2a1b8fb7f6dced3
HDF5_BUILD_DIR = hdf5-$(HDF5_VERSION)
HDF5_INSTALL_DIR = $(PREFIX)/hdf5-$(HDF5_VERSION)
HDF5_CONFIGURE_FLAGS = --enable-cxx
HDF5_CFLAGS = -fPIC
HDF5_CXXFLAGS = -fPIC

.fetch-hdf5:
	@$(RM) $(HDF5_TARBALL)
	$(WGET) $(HDF5_TARBALL_URL)
	@echo '$(HDF5_TARBALL_SHA256)  $(HDF5_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-hdf5: .fetch-hdf5

.extract-hdf5: .fetch-hdf5
	$(RM) $(HDF5_BUILD_DIR)
	$(TAR) -xjf $(HDF5_TARBALL)
	@$(TOUCH) $@

extract-hdf5: .extract-hdf5

.configure-hdf5: .extract-hdf5
	cd $(HDF5_BUILD_DIR) && CFLAGS="$(HDF5_CFLAGS)" CXXFLAGS="$(HDF5_CXXFLAGS)" ./configure $(HDF5_CONFIGURE_FLAGS) --prefix=$(HDF5_INSTALL_DIR)
	@$(TOUCH) $@

configure-hdf5: .configure-hdf5

.build-hdf5: .configure-hdf5
	cd $(HDF5_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-hdf5: .build-hdf5

install-hdf5: .build-hdf5
	cd $(HDF5_BUILD_DIR) && make install

clean-hdf5:
	@$(RM) .build-hdf5
	@$(RM) .configure-hdf5
	@$(RM) .extract-hdf5
	$(RM) $(HDF5_BUILD_DIR)

distclean-hdf5: clean-hdf5
	@$(RM) .fetch-hdf5
	$(RM) $(HDF5_TARBALL)

env-hdf5:
	@echo
	@echo '# add HDF5 $(HDF5_VERSION) to environment'
	@echo 'export PATH="$(HDF5_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export LD_LIBRARY_PATH="$(HDF5_INSTALL_DIR)/lib$${LD_LIBRARY_PATH+:$$LD_LIBRARY_PATH}"'
	@echo 'export CMAKE_PREFIX_PATH="$(HDF5_INSTALL_DIR)$${CMAKE_PREFIX_PATH+:$$CMAKE_PREFIX_PATH}"'

##
## Git version control
##

GIT_VERSION = 1.7.9.1
GIT_TARBALL = git-$(GIT_VERSION).tar.gz
GIT_TARBALL_URL = http://git-core.googlecode.com/files/$(GIT_TARBALL)
GIT_TARBALL_SHA256 = b689a0ddbc99f9a69aef7c81c569289a28ba0787cd27e5e188112e1c3f0e8152
GIT_MANPAGES_TARBALL = git-manpages-$(GIT_VERSION).tar.gz
GIT_MANPAGES_TARBALL_URL = http://git-core.googlecode.com/files/$(GIT_MANPAGES_TARBALL)
GIT_MANPAGES_TARBALL_SHA256 = f31d91061e96b5f882ceed2160d44937b2679931e7e217a66bfe3a23df46adae
GIT_BUILD_DIR = git-$(GIT_VERSION)
GIT_CONFIGURE_FLAGS = --without-python
GIT_INSTALL_DIR = $(PREFIX)/git-$(GIT_VERSION)

.fetch-git:
	@$(RM) $(GIT_TARBALL)
	@$(RM) $(GIT_MANPAGES_TARBALL)
	$(WGET) $(GIT_TARBALL_URL)
	$(WGET) $(GIT_MANPAGES_TARBALL_URL)
	@echo '$(GIT_TARBALL_SHA256)  $(GIT_TARBALL)' | $(SHA256SUM)
	@echo '$(GIT_MANPAGES_TARBALL_SHA256)  $(GIT_MANPAGES_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-git: .fetch-git

.extract-git: .fetch-git
	$(RM) $(GIT_BUILD_DIR)
	$(TAR) -xzf $(GIT_TARBALL)
	@$(TOUCH) $@

extract-git: .extract-git

.configure-git: .extract-git
	cd $(GIT_BUILD_DIR) && ./configure $(GIT_CONFIGURE_FLAGS) --prefix=$(GIT_INSTALL_DIR)
	@$(TOUCH) $@

configure-git: .configure-git

.build-git: .configure-git
	cd $(GIT_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-git: .build-git

install-git: .build-git
	cd $(GIT_BUILD_DIR) && make install
	install -d $(GIT_INSTALL_DIR)/share/man
	cd $(GIT_INSTALL_DIR)/share/man && $(TAR) -xzf $(CURDIR)/$(GIT_MANPAGES_TARBALL)

clean-git:
	@$(RM) .build-git
	@$(RM) .configure-git
	@$(RM) .extract-git
	$(RM) $(GIT_BUILD_DIR)

distclean-git: clean-git
	@$(RM) .fetch-git
	$(RM) $(GIT_TARBALL)
	$(RM) $(GIT_MANPAGES_TARBALL)

env-git:
	@echo
	@echo '# add Git $(GIT_VERSION) to environment'
	@echo 'export PATH="$(GIT_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(GIT_INSTALL_DIR)/share/man$${MANPATH+:$$MANPATH}"'

##
## htop
##

HTOP_VERSION = 1.0.1
HTOP_TARBALL = htop-$(HTOP_VERSION).tar.gz
HTOP_TARBALL_URL = http://sourceforge.net/projects/htop/files/htop/$(HTOP_VERSION)/$(HTOP_TARBALL)
HTOP_TARBALL_SHA256 = 07db2cbe02835f9e186b9610ecc3beca330a5c9beadb3b6069dd0a10561506f2
HTOP_BUILD_DIR = htop-$(HTOP_VERSION)
HTOP_INSTALL_DIR = $(PREFIX)/htop-$(HTOP_VERSION)

.fetch-htop:
	@$(RM) $(HTOP_TARBALL)
	$(WGET) $(HTOP_TARBALL_URL)
	@echo '$(HTOP_TARBALL_SHA256)  $(HTOP_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-htop: .fetch-htop

.extract-htop: .fetch-htop
	$(RM) $(HTOP_BUILD_DIR)
	$(TAR) -xzf $(HTOP_TARBALL)
	@$(TOUCH) $@

extract-htop: .extract-htop

.configure-htop: .extract-htop
	cd $(HTOP_BUILD_DIR) && ./configure --prefix=$(HTOP_INSTALL_DIR)
	@$(TOUCH) $@

configure-htop: .configure-htop

.build-htop: .configure-htop
	cd $(HTOP_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-htop: .build-htop

install-htop: .build-htop
	cd $(HTOP_BUILD_DIR) && make install

clean-htop:
	@$(RM) .build-htop
	@$(RM) .configure-htop
	@$(RM) .extract-htop
	$(RM) $(HTOP_BUILD_DIR)

distclean-htop: clean-htop
	@$(RM) .fetch-htop
	$(RM) $(HTOP_TARBALL)

env-htop:
	@echo
	@echo '# add htop $(HTOP_VERSION) to environment'
	@echo 'export PATH="$(HTOP_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(HTOP_INSTALL_DIR)/share/man$${MANPATH+:$$MANPATH}"'

##
## python-sphinx
##

PYTHON_SPHINX_VERSION = 1.1.2
PYTHON_SPHINX_TARBALL = Sphinx-$(PYTHON_SPHINX_VERSION).tar.gz
PYTHON_SPHINX_TARBALL_URL = http://pypi.python.org/packages/source/S/Sphinx/$(PYTHON_SPHINX_TARBALL)
PYTHON_SPHINX_TARBALL_SHA256 = cf66ee61eef61b7c478907282bddcdd5e04eebd69a00a2bb93881427938fe688
PYTHON_SPHINX_BUILD_DIR = Sphinx-$(PYTHON_SPHINX_VERSION)
PYTHON_SPHINX_INSTALL_DIR = $(PREFIX)/python-sphinx-$(PYTHON_SPHINX_VERSION)
PYTHON_SPHINX_PYTHONPATH = $(PYTHON_SPHINX_INSTALL_DIR)/lib/python

.fetch-python-sphinx:
	@$(RM) $(PYTHON_SPHINX_TARBALL)
	$(WGET) $(PYTHON_SPHINX_TARBALL_URL)
	@echo '$(PYTHON_SPHINX_TARBALL_SHA256)  $(PYTHON_SPHINX_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-python-sphinx: .fetch-python-sphinx

.extract-python-sphinx: .fetch-python-sphinx
	$(RM) $(PYTHON_SPHINX_BUILD_DIR)
	$(TAR) -xzf $(PYTHON_SPHINX_TARBALL)
	@$(TOUCH) $@

extract-python-sphinx: .extract-python-sphinx

.build-python-sphinx: .extract-python-sphinx
	cd $(PYTHON_SPHINX_BUILD_DIR) && python setup.py build
	@$(TOUCH) $@

build-python-sphinx: .build-python-sphinx

install-python-sphinx: .build-python-sphinx
	install -d $(PYTHON_SPHINX_PYTHONPATH)
	cd $(PYTHON_SPHINX_BUILD_DIR) && PYTHONPATH="$(PYTHON_SPHINX_PYTHONPATH)$${PYTHONPATH+:$$PYTHONPATH}" python setup.py install --home=$(PYTHON_SPHINX_INSTALL_DIR)

clean-python-sphinx:
	@$(RM) .build-python-sphinx
	@$(RM) .extract-python-sphinx
	$(RM) $(PYTHON_SPHINX_BUILD_DIR)

distclean-python-sphinx: clean-python-sphinx
	@$(RM) .fetch-python-sphinx
	$(RM) $(PYTHON_SPHINX_TARBALL)

env-python-sphinx:
	@echo
	@echo '# add python-sphinx $(PYTHON_SPHINX_VERSION) to environment'
	@echo 'export PATH="$(PYTHON_SPHINX_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export PYTHONPATH="$(PYTHON_SPHINX_PYTHONPATH)$${PYTHONPATH+:$$PYTHONPATH}"'

##
## Doxygen
##

DOXYGEN_VERSION = 1.7.6.1
DOXYGEN_TARBALL = doxygen-$(DOXYGEN_VERSION).src.tar.gz
DOXYGEN_TARBALL_URL = http://ftp.stack.nl/pub/users/dimitri/$(DOXYGEN_TARBALL)
DOXYGEN_TARBALL_SHA256 = 0e60e794fb172d3fa4a9a9535f0b8e0eeb04e8366153f6b417569af0bcd61fcd
DOXYGEN_BUILD_DIR = doxygen-$(DOXYGEN_VERSION)
DOXYGEN_INSTALL_DIR = $(PREFIX)/doxygen-$(DOXYGEN_VERSION)

.fetch-doxygen:
	@$(RM) $(DOXYGEN_TARBALL)
	$(WGET) $(DOXYGEN_TARBALL_URL)
	@echo '$(DOXYGEN_TARBALL_SHA256)  $(DOXYGEN_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-doxygen: .fetch-doxygen

.extract-doxygen: .fetch-doxygen
	$(RM) $(DOXYGEN_BUILD_DIR)
	$(TAR) -xzf $(DOXYGEN_TARBALL)
	@$(TOUCH) $@

extract-doxygen: .extract-doxygen

.configure-doxygen: .extract-doxygen
	cd $(DOXYGEN_BUILD_DIR) && ./configure --prefix $(DOXYGEN_INSTALL_DIR)
	@$(TOUCH) $@

configure-doxygen: .configure-doxygen

.build-doxygen: .configure-doxygen
	cd $(DOXYGEN_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-doxygen: .build-doxygen

install-doxygen: .build-doxygen
	cd $(DOXYGEN_BUILD_DIR) && make install

clean-doxygen:
	@$(RM) .build-doxygen
	@$(RM) .configure-doxygen
	@$(RM) .extract-doxygen
	$(RM) $(DOXYGEN_BUILD_DIR)

distclean-doxygen: clean-doxygen
	@$(RM) .fetch-doxygen
	$(RM) $(DOXYGEN_TARBALL)

env-doxygen:
	@echo
	@echo '# add Doxygen $(DOXYGEN_VERSION) to environment'
	@echo 'export PATH="$(DOXYGEN_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(DOXYGEN_INSTALL_DIR)/man$${MANPATH+:$$MANPATH}"'

##
## Graphviz
##

GRAPHVIZ_VERSION = 2.28.0
GRAPHVIZ_TARBALL = graphviz-$(GRAPHVIZ_VERSION).tar.gz
GRAPHVIZ_TARBALL_URL = http://www.graphviz.org/pub/graphviz/stable/SOURCES/$(GRAPHVIZ_TARBALL)
GRAPHVIZ_TARBALL_SHA256 = d3aa7973c578cae4cc26d9d6498c57ed06680cab9a4e940d0357a3c6527afc76
GRAPHVIZ_BUILD_DIR = graphviz-$(GRAPHVIZ_VERSION)
GRAPHVIZ_CONFIGURE_FLAGS = --with-qt=no --enable-swig=no --enable-python=no
GRAPHVIZ_INSTALL_DIR = $(PREFIX)/graphviz-$(GRAPHVIZ_VERSION)

.fetch-graphviz:
	@$(RM) $(GRAPHVIZ_TARBALL)
	$(WGET) $(GRAPHVIZ_TARBALL_URL)
	@echo '$(GRAPHVIZ_TARBALL_SHA256)  $(GRAPHVIZ_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-graphviz: .fetch-graphviz

.extract-graphviz: .fetch-graphviz
	$(RM) $(GRAPHVIZ_BUILD_DIR)
	$(TAR) -xzf $(GRAPHVIZ_TARBALL)
	@$(TOUCH) $@

extract-graphviz: .extract-graphviz

.configure-graphviz: .extract-graphviz
	cd $(GRAPHVIZ_BUILD_DIR) && ./configure $(GRAPHVIZ_CONFIGURE_FLAGS) --prefix=$(GRAPHVIZ_INSTALL_DIR)
	@$(TOUCH) $@

configure-graphviz: .configure-graphviz

.build-graphviz: .configure-graphviz
	cd $(GRAPHVIZ_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-graphviz: .build-graphviz

install-graphviz: .build-graphviz
	cd $(GRAPHVIZ_BUILD_DIR) && make install

clean-graphviz:
	@$(RM) .build-graphviz
	@$(RM) .configure-graphviz
	@$(RM) .extract-graphviz
	$(RM) $(GRAPHVIZ_BUILD_DIR)

distclean-graphviz: clean-graphviz
	@$(RM) .fetch-graphviz
	$(RM) $(GRAPHVIZ_TARBALL)

env-graphviz:
	@echo
	@echo '# add Graphviz $(GRAPHVIZ_VERSION) to environment'
	@echo 'export PATH="$(GRAPHVIZ_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(GRAPHVIZ_INSTALL_DIR)/share/man$${MANPATH+:$$MANPATH}"'

##
## Clang C++ compiler
##

CLANG_VERSION = 3.0
LLVM_TARBALL = llvm-$(CLANG_VERSION).tar.gz
LLVM_TARBALL_URL = http://llvm.org/releases/$(CLANG_VERSION)/$(LLVM_TARBALL)
LLVM_TARBALL_SHA256 = 519eb11d3499ce99c6ffdb8718651fc91425ed7690eac91c8d6853474f7c0477
CLANG_TARBALL = clang-$(CLANG_VERSION).tar.gz
CLANG_TARBALL_URL = http://llvm.org/releases/$(CLANG_VERSION)/$(CLANG_TARBALL)
CLANG_TARBALL_SHA256 = b64e72da356d7c3428cfd7ac620d49ec042c84eaee13c26024879f555f4e126d
CLANG_BUILD_DIR = llvm-$(CLANG_VERSION).src
CLANG_CONFIGURE_FLAGS = --enable-optimized --enable-bindings=none
CLANG_INSTALL_DIR = $(PREFIX)/clang-$(CLANG_VERSION)

.fetch-clang:
	@$(RM) $(LLVM_TARBALL)
	@$(RM) $(CLANG_TARBALL)
	$(WGET) $(LLVM_TARBALL_URL)
	$(WGET) $(CLANG_TARBALL_URL)
	@echo '$(LLVM_TARBALL_SHA256)  $(LLVM_TARBALL)' | $(SHA256SUM)
	@echo '$(CLANG_TARBALL_SHA256)  $(CLANG_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-clang: .fetch-clang

.extract-clang: .fetch-clang
	$(RM) $(CLANG_BUILD_DIR)
	$(TAR) -xzf $(LLVM_TARBALL)
	cd $(CLANG_BUILD_DIR)/tools && $(TAR) -xzf $(CURDIR)/$(CLANG_TARBALL) && mv clang-$(CLANG_VERSION).src clang
	@$(TOUCH) $@

extract-clang: .extract-clang

.configure-clang: .extract-clang
	cd $(CLANG_BUILD_DIR) && ./configure $(CLANG_CONFIGURE_FLAGS) --prefix=$(CLANG_INSTALL_DIR)
	@$(TOUCH) $@

configure-clang: .configure-clang

.build-clang: .configure-clang
	cd $(CLANG_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-clang: .build-clang

install-clang: .build-clang
	cd $(CLANG_BUILD_DIR) && make install

clean-clang:
	@$(RM) .build-clang
	@$(RM) .configure-clang
	@$(RM) .extract-clang
	$(RM) $(CLANG_BUILD_DIR)

distclean-clang: clean-clang
	@$(RM) .fetch-clang
	$(RM) $(LLVM_TARBALL)
	$(RM) $(CLANG_TARBALL)

env-clang:
	@echo
	@echo '# add Clang $(CLANG_VERSION) to environment'
	@echo 'export PATH="$(CLANG_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(CLANG_INSTALL_DIR)/share/man$${MANPATH+:$$MANPATH}"'

##
## GNU Parallel
##

GNU_PARALLEL_VERSION = 20111222
GNU_PARALLEL_TARBALL = parallel-$(GNU_PARALLEL_VERSION).tar.bz2
GNU_PARALLEL_TARBALL_URL = http://ftp.gnu.org/gnu/parallel/$(GNU_PARALLEL_TARBALL)
GNU_PARALLEL_TARBALL_SHA256 = 2bb8eb1f358963eb50d3f9f285b887c378e0a660061e7e3e9b8205bf2c443766
GNU_PARALLEL_BUILD_DIR = parallel-$(GNU_PARALLEL_VERSION)
GNU_PARALLEL_INSTALL_DIR = $(PREFIX)/parallel-$(GNU_PARALLEL_VERSION)

.fetch-gnu-parallel:
	@$(RM) $(GNU_PARALLEL_TARBALL)
	$(WGET) $(GNU_PARALLEL_TARBALL_URL)
	@echo '$(GNU_PARALLEL_TARBALL_SHA256)  $(GNU_PARALLEL_TARBALL)' | $(SHA256SUM)
	@$(TOUCH) $@

fetch-gnu-parallel: .fetch-gnu-parallel

.extract-gnu-parallel: .fetch-gnu-parallel
	$(RM) $(GNU_PARALLEL_BUILD_DIR)
	$(TAR) -xjf $(GNU_PARALLEL_TARBALL)
	@$(TOUCH) $@

extract-gnu-parallel: .extract-gnu-parallel

.configure-gnu-parallel: .extract-gnu-parallel
	cd $(GNU_PARALLEL_BUILD_DIR) && ./configure --prefix=$(GNU_PARALLEL_INSTALL_DIR)
	@$(TOUCH) $@

configure-gnu-parallel: .configure-gnu-parallel

.build-gnu-parallel: .configure-gnu-parallel
	cd $(GNU_PARALLEL_BUILD_DIR) && make $(PARALLEL_BUILD_FLAGS)
	@$(TOUCH) $@

build-gnu-parallel: .build-gnu-parallel

install-gnu-parallel: .build-gnu-parallel
	cd $(GNU_PARALLEL_BUILD_DIR) && make install

clean-gnu-parallel:
	@$(RM) .build-gnu-parallel
	@$(RM) .configure-gnu-parallel
	@$(RM) .extract-gnu-parallel
	$(RM) $(GNU_PARALLEL_BUILD_DIR)

distclean-gnu-parallel: clean-gnu-parallel
	@$(RM) .fetch-gnu-parallel
	$(RM) $(GNU_PARALLEL_TARBALL)

env-gnu-parallel:
	@echo
	@echo '# add GNU Parallel $(GNU_PARALLEL_VERSION) to environment'
	@echo 'export PATH="$(GNU_PARALLEL_INSTALL_DIR)/bin$${PATH+:$$PATH}"'
	@echo 'export MANPATH="$(GNU_PARALLEL_INSTALL_DIR)/share/man$${MANPATH+:$$MANPATH}"'
