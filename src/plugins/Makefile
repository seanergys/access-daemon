all: all.sub-folders

install: install.sub-folders

clean: clean.sub-folders

%.sub-folders:
	@ $(MAKE) -C 'general' $* || exit
	@ $(MAKE) -C 'knobs_actuators' $* || exit
	@ $(MAKE) -C 'metrics' $* || exit
	@ $(MAKE) -C 'preloads' $* || exit
