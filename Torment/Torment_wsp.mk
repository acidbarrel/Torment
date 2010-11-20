.PHONY: clean All

All:
	@echo ----------Building project:[ Torment - Debug ]----------
	@"$(MAKE)" -f "Torment.mk"
clean:
	@echo ----------Cleaning project:[ Torment - Debug ]----------
	@"$(MAKE)" -f "Torment.mk" clean
