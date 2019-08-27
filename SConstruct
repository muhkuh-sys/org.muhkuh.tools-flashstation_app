# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------#
#   Copyright (C) 2019 by Christoph Thelen                                #
#   doc_bacardi@users.sourceforge.net                                     #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
#-------------------------------------------------------------------------#


import os.path

#----------------------------------------------------------------------------
#
# Set up the Muhkuh Build System.
#
SConscript('mbs/SConscript')
Import('atEnv')

# Create a build environment for the Cortex-R7 and Cortex-A9 based netX chips.
env_cortexR7 = atEnv.DEFAULT.CreateEnvironment(['gcc-arm-none-eabi-4.9', 'asciidoc'])
env_cortexR7.CreateCompilerEnv('NETX4000', ['arch=armv7', 'thumb'], ['arch=armv7-r', 'thumb'])

# Build the platform libraries.
SConscript('platform/SConscript')


#----------------------------------------------------------------------------
# This is the list of sources. The elements must be separated with whitespace
# (i.e. spaces, tabs, newlines). The amount of whitespace does not matter.
flashapp_sources = """
	src/main.c
	src/init.S
	src/options.c
	src/progress_bar.c
	src/rng.c
	src/sha384.c
	src/snuprintf.c
	src/networking/driver/drv_eth_xc.c
	src/networking/driver/rpu_eth0.c
	src/networking/driver/rpu_eth1.c
	src/networking/driver/rpu_eth2.c
	src/networking/driver/rpu_eth3.c
	src/networking/driver/tpu_eth0.c
	src/networking/driver/tpu_eth1.c
	src/networking/driver/tpu_eth2.c
	src/networking/driver/tpu_eth3.c
	src/networking/driver/xpec_eth_std_mac_rpec0.c
	src/networking/driver/xpec_eth_std_mac_rpec1.c
	src/networking/driver/xpec_eth_std_mac_rpec2.c
	src/networking/driver/xpec_eth_std_mac_rpec3.c
	src/networking/driver/xpec_clean_tpec0.c
	src/networking/driver/xpec_clean_tpec1.c
	src/networking/driver/xpec_clean_tpec2.c
	src/networking/driver/xpec_clean_tpec3.c
	src/networking/stack/arp.c
	src/networking/stack/buckets.c
	src/networking/stack/checksum.c
	src/networking/stack/dhcp.c
	src/networking/stack/dns.c
	src/networking/stack/eth.c
	src/networking/stack/icmp.c
	src/networking/stack/ipv4.c
	src/networking/stack/tftp.c
	src/networking/stack/udp.c
"""


#----------------------------------------------------------------------------
#
# Get the source code version from the VCS.
#
atEnv.DEFAULT.Version('targets/version/version.h', 'templates/version.h')


#----------------------------------------------------------------------------
#
# Create the compiler environments.
#
astrCommonIncludePaths = ['src', '#flasher_lib/includes', '#platform/src', '#platform/src/lib', 'targets/version']

tEnv_netx4000 = atEnv.NETX4000.Clone()
tEnv_netx4000.Replace(LDFILE = File('src/netx4000/netx4000.ld'))
tEnv_netx4000.Append(CPPPATH = astrCommonIncludePaths)

tLib_netx4000 = File('flasher_lib/libflasher_netx4000.a')
tSrc_netx4000 = tEnv_netx4000.SetBuildPath('targets/netx4000', 'src', flashapp_sources)
tElf_netx4000 = tEnv_netx4000.Elf('targets/netx4000/flashapp.elf', tSrc_netx4000 + [tLib_netx4000] + tEnv_netx4000['PLATFORM_LIBRARY'])
tTxt_netx4000 = tEnv_netx4000.ObjDump('targets/netx4000/flashapp_netx4000.txt', tElf_netx4000, OBJDUMP_FLAGS=['--disassemble', '--source', '--all-headers', '--wide'])
tImg_netx4000 = tEnv_netx4000.HBootImage('targets/netx4000/flashapp_netx4000.img', 'src/netx4000/flashapp.xml', HBOOTIMAGE_KNOWN_FILES=dict({'tElfCR7': tElf_netx4000}))
