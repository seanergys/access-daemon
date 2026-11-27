# SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
#
# SPDX-License-Identifier: Apache-2.0

all: all.sub-folders

install: install.sub-folders

clean: clean.sub-folders

%.sub-folders:
	@ $(MAKE) -C 'general' $* || exit
	@ $(MAKE) -C 'knobs_actuators' $* || exit
	@ $(MAKE) -C 'metrics' $* || exit
