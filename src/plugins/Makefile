all: all.sub-folders

clean: clean.sub-folders

%.sub-folders:
	@ $(MAKE) -C 'general' $* || exit
	@ $(MAKE) -C 'metrics' $* || exit