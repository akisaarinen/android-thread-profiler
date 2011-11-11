# This file is part of Android Thread Profiler (ATP)
# 
# Copyright (C) 2011, Aki Saarinen.
#
# ATP is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# ATP is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ATP. If not, see <http://www.gnu.org/licenses/>.

include $(PWD)/config

AGCC:=$(PWD)/../agcc/agcc
ARM_EABI_PATH:=$(ANDROID_PATH)/prebuilt/linux-x86/toolchain/arm-eabi-$(ARM_EABI_VERSION)
PATH:=$(ARM_EABI_PATH)/bin:$(PATH)
TARGET_ANDROID:=procstat-android
TARGET_HOST:=procstat-host
SRC:=procstat.c thread.c thread-list.c read-proc.c

HOST_CC:=gcc

all: compile host

compile:
	test -d $(ANDROID_PATH) || exit 1
	test -d $(ARM_EABI_PATH) || exit 1
	$(AGCC) $(SRC) -o $(TARGET_ANDROID)

host:
	$(HOST_CC) $(SRC) -o $(TARGET_HOST)

push:
	adb push $(TARGET_ANDROID) $(PUSH_DIR)

clean-push: clean compile push

clean:
	rm -rf $(TARGET_ANDROID) $(TARGET_HOST)

