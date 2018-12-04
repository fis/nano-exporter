# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include $(TOPDIR)/rules.mk

PKG_NAME:=nano-exporter
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_SOURCE:=nano-exporter-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://zem.fi/tmp/
#PKG_SOURCE_URL:=https://github.com/fis/nano-exporter/releases/download/1.0.0/
PKG_HASH:=5ab1193f1224231bb21bbfae805ac274f42167045b9150400d5ad3f0d75819c1
PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE

PKG_BUILD_DIR:=$(BUILD_DIR)/nano-exporter-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/nano-exporter
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=Nano exporter for Prometheus
	URL:=https://github.com/fis/nano-exporter/
endef

define Package/nano-exporter/description
Provides node metrics for Prometheus scraping.

The exported metrics are similar to the official Prometheus node_exporter, and
the OpenWrt prometheus-node-exporter-lua package, but this rewrite is a
standalone program with no dependencies.
endef

define Build/Install
endef

define Package/nano-exporter/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/nano-exporter $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/nano-exporter.init $(1)/etc/init.d/nano-exporter
endef

define Package/nano-exporter/postinst
  #!/bin/sh
  if [ -z "$${IPKG_INSTROOT}" ]; then
    echo "Enabling rc.d symlink for nano-exporter"
    /etc/init.d/nano-exporter enable
  fi
  exit 0
endef

define Package/nano-exporter/prerm
  #!/bin/sh
  if [ -z "$${IPKG_INSTROOT}" ]; then
    echo "Removing rc.d symlink for nano-exporter"
    /etc/init.d/nano-exporter disable
  fi
  exit 0
endef

$(eval $(call BuildPackage,nano-exporter))
