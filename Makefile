#
# Makefile:
#	wiringPi - Wiring Compatable library for the Raspberry Pi
#
#	Copyright (c) 2012 Gordon Henderson
#################################################################################
# This file is part of wiringPi:
#	https://projects.drogon.net/raspberry-pi/wiringpi/
#
#    wiringPi is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Lesser General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    wiringPi is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Lesser General Public License for more details.
#
#    You should have received a copy of the GNU Lesser General Public License
#    along with wiringPi.  If not, see <http://www.gnu.org/licenses/>.
#################################################################################

DESTDIR=/usr
PREFIX=/local

CC	= gcc
CFLAGS	= -O2 -Wall -Winline -pipe -fPIC
LIBS    = -lwiringPi
LIBSEARCH = -L/usr/local/lib -L/lib -L/usr/lib
TARGET    = wake_proxyd

SRC	= $(TARGET).c

all-redirect: all

$(TARGET) : $(SRC)
	$(CC) $(CFLAGS) $< $(LIBSEARCH) $(LIBS) -o $@

.PHONEY: clean install uninstall

all: $(TARGET)

clean:
	$(RM) $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(DESTDIR)$(PREFIX)/bin

uninstall:
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

