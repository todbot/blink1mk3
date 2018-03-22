CFLAGS = $(IFLAGS) \
				 -DEFM32HG309F64 \
				 -Wall \
				 -Wextra \
				 -mcpu=cortex-m0plus \
				 -mthumb \
				 -ffunction-sections \
				 -fdata-sections \
				 -fomit-frame-pointer \
				 -std=c99 \
         --specs=nano.specs \
				 -MMD \
				 -MP \
				 -Os \
				 -g 


LSCRIPT = ../tomu.ld

LFLAGS = -mcpu=cortex-m0plus \
				 -mthumb \
				 -T$(LSCRIPT) \
				 --specs=nosys.specs \
				 -Wl,--gc-sections \
				 -Wl,--start-group \
				 -lnosys \
				 -Wl,--end-group
