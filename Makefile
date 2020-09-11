SVR_PREFIX =
AS_PREFIX =
CLT_PREFIX =

SVR_CC = $(SVR_PREFIX)gcc
AS_CC = $(AS_PREFIX)gcc
CLT_CC = $(CLT_PREFIX)gcc

TARGET_PATH = out

MKDIR = mkdir -p
RM = rm -rf

SVR_TARGET = server
AS_TARGET = asdroid
CLT_TARGET = client

SVR_SRC = src/server/server_main.c
AS_SRC = src/asdroid/asdroid_main.c src/asdroid/capture.c src/lib/crypto.c
CLT_SRC = src/client/client_main.c src/lib/crypto.c

LINK = -lpthread -lssl -lcrypto -lm

INC = -Iinc \
      -Isrc/lib/openssl/

all: $(TARGET_PATH)/$(SVR_TARGET) $(TARGET_PATH)/$(AS_TARGET) $(TARGET_PATH)/$(CLT_TARGET)

$(TARGET_PATH)/$(SVR_TARGET):$(SVR_SRC)
	@$(MKDIR) $(dir $@)
	$(SVR_CC) -g $(INC) $^ -o $@ $(LINK)

$(TARGET_PATH)/$(AS_TARGET):$(AS_SRC)
	@$(MKDIR) $(dir $@)
	$(AS_CC) -g $(INC) $^ -o $@ $(LINK)

$(TARGET_PATH)/$(CLT_TARGET):$(CLT_SRC)
	@$(MKDIR) $(dir $@)
	$(CLT_CC) -g $(INC) $^ -o $@ $(LINK)

.PHONY: clean
clean:
	$(RM) $(TARGET_PATH)/*
