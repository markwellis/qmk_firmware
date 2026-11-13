wear_leveling_common_DEFS := \
	-DWEAR_LEVELING_TESTS
wear_leveling_common_SRC := \
	$(LIB_PATH)/fnv/qmk_fnv_type_validation.c \
	$(LIB_PATH)/fnv/hash_32a.c \
	$(LIB_PATH)/fnv/hash_64a.c \
	$(QUANTUM_PATH)/wear_leveling/wear_leveling.c \
	$(QUANTUM_PATH)/wear_leveling/tests/backing_mocks.cpp
wear_leveling_common_INC := \
	$(LIB_PATH)/fnv \
	$(QUANTUM_PATH)/wear_leveling

wear_leveling_general_DEFS := \
	$(wear_leveling_common_DEFS) \
	-DBACKING_STORE_WRITE_SIZE=2 \
	-DWEAR_LEVELING_BACKING_SIZE=48 \
	-DWEAR_LEVELING_LOGICAL_SIZE=16
wear_leveling_general_SRC := \
	$(wear_leveling_common_SRC) \
	$(QUANTUM_PATH)/wear_leveling/tests/wear_leveling_general.cpp
wear_leveling_general_INC := \
	$(wear_leveling_common_INC)

wear_leveling_2byte_optimized_writes_DEFS := \
	$(wear_leveling_common_DEFS) \
	-DBACKING_STORE_WRITE_SIZE=2 \
	-DWEAR_LEVELING_BACKING_SIZE=65536 \
	-DWEAR_LEVELING_LOGICAL_SIZE=32768
wear_leveling_2byte_optimized_writes_SRC := \
	$(wear_leveling_common_SRC) \
	$(QUANTUM_PATH)/wear_leveling/tests/wear_leveling_2byte_optimized_writes.cpp
wear_leveling_2byte_optimized_writes_INC := \
	$(wear_leveling_common_INC)

wear_leveling_2byte_DEFS := \
	$(wear_leveling_common_DEFS) \
	-DBACKING_STORE_WRITE_SIZE=2 \
	-DWEAR_LEVELING_BACKING_SIZE=48 \
	-DWEAR_LEVELING_LOGICAL_SIZE=16
wear_leveling_2byte_SRC := \
	$(wear_leveling_common_SRC) \
	$(QUANTUM_PATH)/wear_leveling/tests/wear_leveling_2byte.cpp
wear_leveling_2byte_INC := \
	$(wear_leveling_common_INC)

wear_leveling_4byte_DEFS := \
	$(wear_leveling_common_DEFS) \
	-DBACKING_STORE_WRITE_SIZE=4 \
	-DWEAR_LEVELING_BACKING_SIZE=48 \
	-DWEAR_LEVELING_LOGICAL_SIZE=16
wear_leveling_4byte_SRC := \
	$(wear_leveling_common_SRC) \
	$(QUANTUM_PATH)/wear_leveling/tests/wear_leveling_4byte.cpp
wear_leveling_4byte_INC := \
	$(wear_leveling_common_INC)

wear_leveling_8byte_DEFS := \
	$(wear_leveling_common_DEFS) \
	-DBACKING_STORE_WRITE_SIZE=8 \
	-DWEAR_LEVELING_BACKING_SIZE=48 \
	-DWEAR_LEVELING_LOGICAL_SIZE=16
wear_leveling_8byte_SRC := \
	$(wear_leveling_common_SRC) \
	$(QUANTUM_PATH)/wear_leveling/tests/wear_leveling_8byte.cpp
wear_leveling_8byte_INC := \
	$(wear_leveling_common_INC)

wear_leveling_spi_tests := wear_leveling_spi_2k wear_leveling_spi_4k wear_leveling_spi_8k wear_leveling_spi_block wear_leveling_spi_mixed wear_leveling_spi_offset wear_leveling_spi_subblock
wear_leveling_spi_common_DEFS := -DWEAR_LEVELING_SPI_FLASH -DEXTERNAL_FLASH_SIZE=262144 -DEXTERNAL_FLASH_SPI_SLAVE_SELECT_PIN=1 -DWEAR_LEVELING_LOGICAL_SIZE=1024
wear_leveling_spi_common_SRC := $(DRIVER_PATH)/wear_leveling/wear_leveling_flash_spi.c $(QUANTUM_PATH)/wear_leveling/tests/spi_flash/test_spi_flash.cpp
wear_leveling_spi_common_INC := $(DRIVER_PATH) $(DRIVER_PATH)/wear_leveling $(DRIVER_PATH)/flash $(QUANTUM_PATH)/wear_leveling $(QUANTUM_PATH)/wear_leveling/tests/spi_flash

wear_leveling_spi_2k_DEFS := $(wear_leveling_spi_common_DEFS) -DWEAR_LEVELING_BACKING_SIZE=2048
wear_leveling_spi_4k_DEFS := $(wear_leveling_spi_common_DEFS) -DWEAR_LEVELING_BACKING_SIZE=4096
wear_leveling_spi_8k_DEFS := $(wear_leveling_spi_common_DEFS) -DWEAR_LEVELING_BACKING_SIZE=8192
wear_leveling_spi_block_DEFS := $(wear_leveling_spi_common_DEFS) -DWEAR_LEVELING_BACKING_SIZE=65536
wear_leveling_spi_mixed_DEFS := $(wear_leveling_spi_common_DEFS) -DWEAR_LEVELING_BACKING_SIZE=73728 -DWEAR_LEVELING_EXTERNAL_FLASH_BLOCK_COUNT=2
wear_leveling_spi_offset_DEFS := $(wear_leveling_spi_common_DEFS) -DWEAR_LEVELING_BACKING_SIZE=8192 -DWEAR_LEVELING_EXTERNAL_FLASH_BLOCK_OFFSET=1

wear_leveling_spi_subblock_DEFS := $(wear_leveling_spi_common_DEFS) -DWEAR_LEVELING_BACKING_SIZE=2048 -DEXTERNAL_FLASH_BLOCK_SIZE=4096

$(foreach test,$(wear_leveling_spi_tests),$(eval $(test)_SRC := $(wear_leveling_spi_common_SRC)))
$(foreach test,$(wear_leveling_spi_tests),$(eval $(test)_INC := $(wear_leveling_spi_common_INC)))
