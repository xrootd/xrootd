# Please see SoftRelTools/HOWTO-GNUmakefile for documentation
# $Id$
#################################################################
#++ library products				[build it with 'lib']

LIBREMOVEFILES := DontCompileThis.cc OrThis.F
LIBTMPLFILES := SomeTemplateClass.cc
LIBDDLORDERED := DoSecond.ddl DoFirst.ddl

#################################################################
#++ extra binary products	[not in production, build it with extrabin]

EXTRABINS := extratemp extratemp2

$(addprefix $(bindir),$(EXTRABINS)): $(bindir)% : %.o

#################################################################
#++ binary products				[build it with 'bin']

BINS := $(PACKAGE)App mytesttemp2
BINCCFILES := AppUserBuild.cc testtemp2.cc $(EXTRABINS:=.cc)

#++ Binary rules		 [in production, build it with 'bin']

$(bindir)$(PACKAGE)App: AppUserBuild.o

$(bindir)mytesttemp2: testtemp2.o

#++ shell script products.. 			[build it with 'bin']
#BINSCRIPTS := testscript

#################################################################
#++ regression test scripts			[build it with 'test']

$(testdir)mytest.T : mytest.tcl mytesttemp2

#################################################################
#++ include standard makefile from SoftRelTools.
include SoftRelTools/standard.mk
