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
	src/crc32.c
	src/fdl.c
	src/main.c
	src/init.S
	src/malloc_dummy.c
	src/network_lwip.c
	src/options.c
	src/progress_bar.c
	src/rng.c
	src/sha384.c
	src/wfp.c
	src/networking/driver/drv_eth_xc.c
"""

sources_lwip = """
	src/networking/lwip-2.1.2/src/apps/http/http_client.c

	src/networking/lwip-2.1.2/src/core/ipv4/autoip.c
	src/networking/lwip-2.1.2/src/core/ipv4/dhcp.c
	src/networking/lwip-2.1.2/src/core/ipv4/etharp.c
	src/networking/lwip-2.1.2/src/core/ipv4/icmp.c
	src/networking/lwip-2.1.2/src/core/ipv4/igmp.c
	src/networking/lwip-2.1.2/src/core/ipv4/ip4_addr.c
	src/networking/lwip-2.1.2/src/core/ipv4/ip4_frag.c
	src/networking/lwip-2.1.2/src/core/ipv4/ip4.c
	src/networking/lwip-2.1.2/src/core/altcp_alloc.c
	src/networking/lwip-2.1.2/src/core/altcp_tcp.c
	src/networking/lwip-2.1.2/src/core/altcp.c
	src/networking/lwip-2.1.2/src/core/def.c
	src/networking/lwip-2.1.2/src/core/dns.c
	src/networking/lwip-2.1.2/src/core/inet_chksum.c
	src/networking/lwip-2.1.2/src/core/init.c
	src/networking/lwip-2.1.2/src/core/ip.c
	src/networking/lwip-2.1.2/src/core/mem.c
	src/networking/lwip-2.1.2/src/core/memp.c
	src/networking/lwip-2.1.2/src/core/netif.c
	src/networking/lwip-2.1.2/src/core/pbuf.c
	src/networking/lwip-2.1.2/src/core/raw.c
	src/networking/lwip-2.1.2/src/core/stats.c
	src/networking/lwip-2.1.2/src/core/sys.c
	src/networking/lwip-2.1.2/src/core/tcp_in.c
	src/networking/lwip-2.1.2/src/core/tcp_out.c
	src/networking/lwip-2.1.2/src/core/tcp.c
	src/networking/lwip-2.1.2/src/core/timeouts.c
	src/networking/lwip-2.1.2/src/core/udp.c
	src/networking/lwip-2.1.2/src/netif/ethernet.c
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
astrCommonIncludePaths = [
	'src',
	'src/networking/lwip-2.1.2/src/include',
	'#targets/build_requirements/jonchki/install/lib/includes',
	'#platform/src',
	'#platform/src/lib',
	'targets/version'
]

tEnv_netx4000 = atEnv.NETX4000.Clone()
tEnv_netx4000.CompileDb('targets/netx4000/compile_commands.json')
tEnv_netx4000.Replace(LDFILE = File('src/netx4000/netx4000.ld'))
tEnv_netx4000.Append(CPPPATH = astrCommonIncludePaths)
tEnv_netx4000.Append(CPPDEFINES = [['CFG_INCLUDE_SHA1', '0'], ['CFG_INCLUDE_PARFLASH', '0'], ['CFG_INCLUDE_SDIO', '1']])

tLib_netx4000 = File('#targets/build_requirements/jonchki/install/lib/libflasher_netx4000.a')
tSrc_netx4000 = tEnv_netx4000.SetBuildPath('targets/netx4000', 'src', flashapp_sources + sources_lwip)
tElf_netx4000 = tEnv_netx4000.Elf('targets/netx4000/flashapp.elf', tSrc_netx4000 + [tLib_netx4000] + tEnv_netx4000['PLATFORM_LIBRARY'])
tTxt_netx4000 = tEnv_netx4000.ObjDump('targets/netx4000/flashapp_netx4000.txt', tElf_netx4000, OBJDUMP_FLAGS=['--disassemble', '--source', '--all-headers', '--wide'])
tImg_netx4000 = tEnv_netx4000.HBootImage('targets/netx4000/flashapp_netx4000.img', 'src/netx4000/flashapp.xml', HBOOTIMAGE_KNOWN_FILES=dict({'tElfCR7': tElf_netx4000}))


#----------------------------------------------------------------------------
#
# Build the artifacts.
#
strGroup = PROJECT_GROUP
strModule = PROJECT_MODULE

# Split the group by dots.
aGroup = strGroup.split('.')
# Build the path for all artifacts.
strModulePath = 'targets/jonchki/repository/%s/%s/%s' % ('/'.join(aGroup), strModule, PROJECT_VERSION)

# Set the name of the artifact.
strArtifact0 = 'flashstation_app'

tArcList0 = atEnv.DEFAULT.ArchiveList('zip')
tArcList0.AddFiles('netx/',
    tImg_netx4000)

tArtifact0 = atEnv.DEFAULT.Archive(os.path.join(strModulePath, '%s-%s.zip' % (strArtifact0, PROJECT_VERSION)), None, ARCHIVE_CONTENTS = tArcList0)
