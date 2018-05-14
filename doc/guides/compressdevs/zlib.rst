..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Cavium Networks.

ZLIB Compression Poll Mode Driver
==================================

The ZLIB PMD (**librte_pmd_zlib**) provides poll mode compression &
decompression driver based on SW zlib library,

Features
--------

ZLIB PMD has support for:

Compression/Decompression algorithm:

* DEFLATE

Huffman code type:

* FIXED
* DYNAMIC

Checksum support:

* Adler32
* CRC32

Window size support:

* 32K

Limitations
-----------

* Chained mbufs are not supported.

Installation
------------

* To build DPDK with ZLIB library, the user is required to download the ``libz`` library.
* Use following command for installation.

*  For Fedora users ::
  yum install zlib-devel
*  For ubuntu users ::
  apt-get install zlib1g-dev

* Once downloaded, the user needs to build the library.

* make can  be used to install the library on their system, before building DPDK::

    make
    sudo make install

Initialization
--------------

In order to enable this virtual compression PMD, user must:

* Set ``CONFIG_RTE_LIBRTE_PMD_ZLIB=y`` in config/common_base.

To use the PMD in an application, user must:

* Call ``rte_vdev_init("compress_zlib")`` within the application.

* Use ``--vdev="compress_zlib"`` in the EAL options, which will call ``rte_vdev_init()`` internally.

The following parameter (optional) can be provided in the previous two calls:

* ``socket_id:`` Specify the socket where the memory for the device is going to be allocated
  (by default, socket_id will be the socket where the core that is creating the PMD is running on).
