# Run from the project root; experiment build files stay in their own folder.
all run clean:
	$(MAKE) -C docs/labs/posix $@

.PHONY: all run clean
