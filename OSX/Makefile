# XScreenSaver for MacOS X, Copyright (c) 2006-2015 by Jamie Zawinski.

XCODE_APP = /Applications/Xcode.app

# To build savers that will run on MacOS 10.6 and 10.7, Xcode 5.0.2 must
# be used (as that's the latest version of Xcode that ships with a version
# of clang that implements "-fobjc-gc").  However, Xcode 5.0.2 will not
# launch on MacOS 10.11 or later.
#
# XCODE_APP = /Applications/Xcode-5.0.2.app

TARGETS    = All Savers
#ARCH      = -arch i386 -arch x86_64 ONLY_ACTIVE_ARCH=NO
CERT	   = 'Developer ID Installer: Jamie Zawinski (4627ATJELP)'
PKGID	   = org.jwz.xscreensaver
THUMBDIR   = build/screenshots
XCODEBUILD = $(XCODE_APP)/Contents/Developer/usr/bin/xcodebuild
SETFILE    = $(XCODE_APP)/Contents/Developer/Tools/SetFile
SETICON    = ./seticon.pl

default: release
all: debug release

clean:
	-rm -rf build
#	$(XCODEBUILD) -target "$(TARGETS)" clean

distclean:
	-rm -f config.status config.cache config.log \
	  *.bak *.rej TAGS *~ "#"*
	-rm -rf autom4te*.cache
	-rm -rf build Sparkle.framework

distdepend:: Sparkle.framework
distdepend:: update_plist_version

debug: distdepend
	$(XCODEBUILD) $(ARCH) -target "$(TARGETS)" -configuration Debug   build

release:: distdepend
	$(XCODEBUILD) $(ARCH) -target "$(TARGETS)" -configuration Release build

release:: check_versions

Sparkle.framework:
	rm -rf bin sparkle-bin
	tar -vxjf ../archive/Sparkle-1.21.2.tar.bz2 \
	  --exclude CHANGELOG \
	  --exclude LICENSE \
	  --exclude SampleAppcast.xml \
	  --exclude Sparkle.framework.dSYM \
	  --exclude Sparkle\ Test\ App\*
	mv bin sparkle-bin

# Download and resize images from jwz.org.
# This saves us having to include 4MB of images in the tar file
# that will only be used by a vast minority of people building
# from source.
# update-info-plist.pl runs this as needed.
# Might be better to do this with curl, since that is installed by default.

BASE = xscreensaver/screenshots/
URL = https://www.jwz.org/$(BASE)
WGET = wget -q -U xscreensaver-build-osx --content-on-error=0
CVT  = -thumbnail '200x150^' -gravity center -extent 200x150 \
     \( +clone  -alpha extract \
        -draw 'fill black polygon 0,0 0,6 6,0 fill white circle 6,6 6,0' \
        \( +clone -flip \) -compose Multiply -composite \
        \( +clone -flop \) -compose Multiply -composite \
     \) -alpha off -compose CopyOpacity -composite \
    -colorspace sRGB \
    -strip \
    -quality 95 \
    +dither -colors 128

$(THUMBDIR)/%.png:
	@\
	FILE1=`echo "$@" | sed 's!^.*/\([^/]*\)\.png$$!\1.jpg!'` ;	\
	FILE2="$@" ;							\
	TMP="$$FILE2".tmp ;						\
	URL="$(URL)$$FILE1" ;						\
	URL2="$(URL)retired/$$FILE1" ;					\
	if [ ! -d $(THUMBDIR) ]; then mkdir -p $(THUMBDIR) ; fi ;	\
	rm -f "$$FILE2" "$$TMP" ;					\
	set +e ;							\
	if [ -f "$$HOME/www/$(BASE)/$$FILE1" ]; then			\
	  cp -p "$$HOME/www/$(BASE)/$$FILE1" "$$TMP" ;			\
	else								\
	  echo "downloading $$URL..." ;					\
	  $(WGET) -O"$$TMP" "$$URL" ;					\
	  if [ ! -s "$$TMP" ]; then					\
	    echo "downloading $$URL2..." ;				\
	    $(WGET) -O"$$TMP" "$$URL2" ;				\
	  fi ;								\
	  if [ ! -s "$$TMP" ]; then					\
	    rm -f "$$TMP" ;						\
	    echo "failed: $$URL" ;					\
	    exit 1 ;							\
	  fi ;								\
	fi ;								\
	rm -f "$$FILE2" ;						\
	convert jpg:- $(CVT) "$$FILE2" < "$$TMP" ;			\
	if [ ! -s "$$FILE2" ]; then					\
	  echo "$$FILE2 failed" >&2 ;					\
	  rm -f "$$FILE2" "$$TMP" ;					\
	  exit 1 ;							\
	else								\
	  rm -f "$$TMP" ;						\
	fi


check_versions:
	@\
  SRC=../utils/version.h ;						\
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		\
  DIR=build/Release ;							\
  RESULT=0 ;								\
  for S in $$DIR/*.{saver,app} ; do					\
   for P in $$S/Contents/Info.plist ; do				\
     V2=`plutil -convert xml1 -o - "$$P" |				\
	 perl -0000 -n -e						\
	'm@<key>CFBundleVersion</key>\s*<string>(.*?)</string>@si 	\
	 && print $$1'` ;						\
     if [ "$$V2" != "$$V" ] ; then					\
       echo "Wrong version: $$S ($$V2)" ;				\
       RESULT=1 ;							\
     fi ;								\
   done ;								\
  done ;								\
  if [ "$$RESULT" = 0 ]; then echo "Versions match ($$V2)" ; fi ;	\
  exit $$RESULT


check_gc:
	@\
  DIR="build/Release" ;							\
  RESULT=0 ;								\
  for S in "$$DIR/"*.saver ; do						\
   SS=`echo "$$S" | sed -e 's@^.*/@@' -e 's/.saver$$//'` ;		\
   D="$$S/Contents/MacOS/$$SS" ;					\
   V=`otool -s __DATA __objc_imageinfo "$$D"				\
      | grep ' 00 02 00 '` ;						\
   if [ -z "$$V" ]; then						\
     echo "$$S does not have GC enabled" ;				\
     RESULT=1 ;								\
   fi ;									\
  done ;								\
									\
  for D in "$$DIR"/webcollage-helper					\
	   "$$DIR"/*.saver/Contents/*/webcollage-helper			\
	   "$$DIR"/*.app/Contents/*/XScreenSaverUpdater			\
   ; do									\
   V=`otool -s __DATA __objc_imageinfo "$$D"				\
      | grep ' 00 02 00 '` ;						\
   if [ ! -z "$$V" ]; then						\
     echo "$$D has GC enabled" ;					\
     RESULT=1 ;								\
   fi ;									\
  done ;								\
									\
  if [ "$$RESULT" = 0 ]; then echo "GC enabled" ; fi ;			\
  exit $$RESULT


check_coretext:
	@\
  DIR="build/Release" ;							\
  RESULT=0 ;								\
  for S in "$$DIR/"*.{saver,app} ; do					\
   SS=`echo "$$S" | sed -e 's@^.*/@@' -e 's/[.][a-z]*$$//'` ;		\
   D="$$S/Contents/MacOS/$$SS" ;					\
   FF=`otool -l "$$D"							\
      | fgrep '/CoreText.framework/'					\
      | sed -n 's/^ *name \([^ ]*\).*$$/\1/p'` ;			\
   if [ -z "$$FF" ] ; then						\
     echo "$$S not linked with CoreText" >/dev/null ;			\
   else 								\
     OK=`echo "$$FF" | fgrep -v '/ApplicationServices.framework/'` ;	\
     if [ ! -z "$$OK" ]; then						\
       echo "$$S is linked with the wrong CoreText: $$FF" ;		\
       RESULT=1 ;							\
     else								\
       echo "$$S linked right: $$FF" >/dev/null ;			\
     fi ;								\
   fi ;									\
  done ;								\
  if [ "$$RESULT" = 0 ]; then echo "CoreText linked correctly" ; fi ;	\
  exit $$RESULT


# Arrrrgh
ios-function-table.m::
	@./build-fntable.pl build/Debug-iphonesimulator/XScreenSaver.app $@


echo_tarfiles:
	@echo `( find .							\
	  \( \( -name '.??*' -o -name build -o -name CVS -o -name '*~*'	\
	     -o -name 'jwz.*' -o -name 'Screen Savers'			\
             -o -name xscreensaver.xcodeproj \)				\
	     -prune \)							\
	  -o \( -type f -o -type l \) -print ;				\
	  echo xscreensaver.xcodeproj/project.pbxproj )			\
	| sed 's@^\./@@'						\
	| sort`

update_plist_version:
	@								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		      \
  T=/tmp/xs.$$$$ ;							      \
  for S in *.plist ; do							      \
    /bin/echo -n "Updating version number in $$S to \"$$V\"... " ;	      \
    KEYS="CFBundleVersion|CFBundleShortVersionString|CFBundleLongVersionString|CFBundleGetInfoString|NSHumanReadableCopyright" ; \
    perl -0777 -pne							      \
      "s@(<key>($$KEYS)</key>\s*<string>)[^<>]+(</string>)@\$${1}$$V\$${3}@g" \
      < $$S > $$T ;							      \
    if cmp -s $$S $$T ; then						      \
      echo "unchanged." ;						      \
    else								      \
      cat $$T > $$S ;							      \
      echo "done." ;							      \
    fi ;								      \
  done ;								      \
  rm $$T


updates.xml::
	./updates.pl xscreensaver ../README ../archive ~/www/xscreensaver
	@$(MAKE) test_sig

test_sig::
	@								    \
  U=../utils/version.h ;						    \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ;]*\).*/\1/p' < $$U` ;		    \
  BASE="xscreensaver-$$V" ;						    \
  OUTDIR="../archive" ;							    \
  DMG="$$OUTDIR/$$BASE.dmg" ;						    \
  SIG=`sed -n 's/^.*dsaSignature="\(.*\)".*/\1/p' updates.xml` ;	    \
  PUB="sparkle_dsa_pub.pem" ;						    \
  NN="t.$$$$" ;								    \
  SIGB=/tmp/$$NN.sig ;							    \
  HASH=/tmp/$$NN.hash ;							    \
  rm -f "$$SIGB" "$$HASH" ;						    \
  echo "$$SIG " | base64 -D  > "$$SIGB" ;				    \
  set -e ;								    \
  for OPENSSL in /usr/bin/openssl /opt/local/bin/openssl ; do		    \
    $$OPENSSL dgst -sha1 -binary  < "$$DMG"  > "$$HASH" ;		    \
    /bin/echo -n "$$OPENSSL	`$$OPENSSL version`:	" ;		    \
    $$OPENSSL dgst -dss1 -verify "$$PUB" -signature "$$SIGB" "$$HASH" ;	    \
  done ;								    \
  rm -f "$$SIGB" "$$HASH" ;						    \


build/Release/installer.pkg: installer.rtf installer.xml installer.sh installer.png ../utils/version.h
	@\
  set -e ;								\
  SRC=../utils/version.h ;						\
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		\
									\
  DIST="installer.xml" ;						\
  STAGE="build/Release/pkg_stage" ;					\
  FINAL="$@" ;								\
  UNSIGNED="$$STAGE/contents.pkg" ;					\
  PRODUCT="$$STAGE/product_unsigned.pkg" ;				\
  SCRIPTS="$$STAGE/scripts" ;						\
  RES="$$STAGE/resources" ;						\
									\
  set -x ;								\
  rm -rf "$$STAGE" ;							\
  mkdir -p "$$SCRIPTS" "$$RES" ;					\
									\
  cp -p installer.sh "$$SCRIPTS/preinstall" ;				\
  cp -p installer.png "$$RES/background.png" ;				\
  cp -p installer.rtf "$$RES/welcome.rtf" ;				\
									\
  pkgbuild --identifier "$(PKGID)" --version "$$V"			\
	   --scripts "$$SCRIPTS" --nopayload "$$UNSIGNED" ;		\
									\
  productbuild --distribution "$$DIST" --resources "$$RES"		\
	       --package-path "$$STAGE" --version "$$V" "$$PRODUCT" ;	\
									\
  productsign --sign $(CERT) "$$PRODUCT" "$$FINAL" ;			\
  spctl --assess --verbose=4 --type install "$$FINAL" ;			\
									\
  rm -rf "$$STAGE" ;							\


# -format UDBZ saves 4% (~1.2 MB) over UDZO.
dmg:: distdepend check_versions check_coretext
#dmg:: check_gc
dmg:: build/Release/installer.pkg
dmg:: _dmg notarize

_dmg::
	@								      \
  set -e ;								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		      \
  TMPDIR="build" ;							      \
  SRC="build/Release" ;							      \
  EXTRAS=../../xdaliclock/OSX/build/Release/*.saver ;			      \
  BASE="xscreensaver-$$V" ;						      \
  OUTDIR="../archive" ;							      \
  DMG="$$OUTDIR/$$BASE.dmg" ;						      \
  TMPDMG="$$TMPDIR/tmp.dmg" ;						      \
  VOLNAME="XScreenSaver $$V" ;						      \
  STAGE="$$TMPDIR/dmg_stage" ;						      \
  DST="$$STAGE/Screen Savers" ;						      \
  PKG="$$STAGE/Install Everything.pkg" ;				      \
  rm -f "$$DMG" ;							      \
  rm -rf "$$STAGE" ;							      \
  echo + mkdir -p "$$DST" ;						      \
         mkdir -p "$$DST" ;						      \
									      \
  retired=`perl -0 -ne							      \
     's/\\\\\\n//g; m/^RETIRED_EXES\s*=\s*(.*)$$/m && print "$$1\n"'	      \
      ../hacks/Makefile.in ;						      \
      perl -0 -ne							      \
     's/\\\\\\n//g; m/^RETIRED_GL_EXES\s*=\s*(.*)$$/m && print "$$1\n"'	      \
      ../hacks/glx/Makefile.in ;					      \
     echo xscreensaver ;						      \
     echo savertester` ;						      \
									      \
  for f in $$SRC/*.{saver,app} $$EXTRAS ; do				      \
   f2=`basename "$$f"` ;						      \
   ok=yes ;								      \
   ff=`echo $$f | perl -e '$$_=<>; s@^.*/(.*)\..*$$@\L$$1@; print'`;	      \
   for r in $$retired ; do						      \
    if [ "$$ff" = "$$r" ]; then ok=no ; fi ;				      \
   done ;								      \
   if [ "$$ff" = testx11 ]; then ok=no ; fi ;				      \
   if [ "$$f2" = "XScreenSaverUpdater.app" ]; then			      \
    DST_HACK="XScreenSaver.updater" ;					      \
    echo + tar ... "$$DST/$$DST_HACK" ;					      \
    ( cd $$SRC ; tar -czf - "$$f2" ) > "$$DST/$$DST_HACK" ;		      \
   elif [ "$$ok" = yes ]; then						      \
    echo + cp -pR "$$f" "$$DST/" ;					      \
           cp -pR "$$f" "$$DST/" ;					      \
   else									      \
    echo skipping "$$f" ;						      \
   fi ;									      \
  done ;								      \
									      \
  set -x ;								      \
  cp -p bindist.rtf "$$STAGE/Read Me.rtf" ;				      \
  cp -p build/Release/installer.pkg "$$PKG" ;				      \
  cp -p bindist-DS_Store "$$STAGE/.DS_Store" ;				      \
  cp -p bindist*.webloc "$$STAGE/" ;					      \
  cp -p XScreenSaverDMG.icns "$$STAGE/.VolumeIcon.icns" ;		      \
  ${SETFILE} -a C "$$STAGE" ;						      \
  ${SETFILE} -a E "$$STAGE"/*.{rtf,pkg,webloc} ;			      \
  $(SETICON) -d ../../xdaliclock/OSX/daliclockSaver.icns		      \
		"$$DST/DaliClock.saver" ;				      \
  $(SETICON) -d XScreenSaverFolder.icns "$$DST" ;			      \
  $(SETICON) -d XScreenSaver.icns "$$DST"/*.saver ;			      \
  $(SETICON) -d SaverRunner.icns "$$DST"/*.app ;			      \
  $(SETICON) -d XScreenSaverWebloc.icns        "$$STAGE"/bindist.webloc ;     \
  $(SETICON) -d XScreenSaverAndroidWebloc.icns "$$STAGE"/bindist2.webloc ;    \
  $(SETICON) -d XScreenSaverPkg.icns "$$STAGE"/*.pkg ;			      \
  mv "$$STAGE/bindist.webloc"  "$$STAGE/Get the iPhone:iPad Version.webloc" ; \
  mv "$$STAGE/bindist2.webloc" "$$STAGE/Get the Android Version.webloc" ;     \
									      \
  set +x ;								      \
  echo "Checking signatures..." ;					      \
  spctl --assess --type install "$$PKG" ;				      \
  spctl --assess --type execute "$$SRC/XScreenSaverUpdater.app" ;	      \
  spctl --assess --type execute "$$DST/"*.app ;				      \
  spctl --assess --type install "$$DST/"*.saver ;			      \
  set -x ;								      \
									      \
  hdiutil makehybrid -quiet -ov -hfs -hfs-volume-name "$$VOLNAME"	      \
    -hfs-openfolder "$$STAGE" "$$STAGE" -o "$$TMPDMG" ;			      \
  rm -rf "$$STAGE" ;							      \
									      \
  hdiutil convert -quiet -ov -format UDBZ -imagekey zlib-level=9	      \
    "$$TMPDMG" -o "$$DMG" ;						      \
  xattr -w com.apple.quarantine "0000;00000000;;" "$$DMG" ;		      \
  rm -f "$$TMPDMG" ;							      \
  ls -ldhgF "$$DMG" ;							      \
  $(MAKE) notarize ;							      \


# To set up notarization:
#  - Log in on https://appleid.apple.com/
#  - Generate App-Specific Password, "altool-notarizer"
#  - Keychain Access / New
#  - Name: "altool-notarizer", Account: "jwz@jwz.org",
#    Pass: the one you just generated.
#
# "make notarize", which will upload the DMG (slow).
# A response will be emailed back in about an hour.
# When that arrives, "make staple".
#
# https://developer.apple.com/documentation/security/notarizing_your_app_before_distribution/customizing_the_notarization_workflow?language=objc
#
# Note that if Sparkle.framework/.../Autoupdate.app is not independently
# signed, it won't pass. I had to add a build phase for that.
#
NOTARGS=-u "jwz@jwz.org" -p "@keychain:altool-notarizer"

notarize::
	@								      \
  set -e ;								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		      \
  BASE="xscreensaver-$$V" ;						      \
  OUTDIR="../archive" ;							      \
  DMG="$$OUTDIR/$$BASE.dmg" ;						      \
  set -x ;								      \
  xcrun altool --notarize-app --primary-bundle-id "org.jwz.xscreensaver"      \
    --file "$$DMG" $(NOTARGS) ;						      \
  echo "" ;								      \
  sleep 3 ;								      \
  $(MAKE) notarize_wait staple updates.xml

notarize_wait::
	@while ( xcrun altool --notarization-history 0 $(NOTARGS) |	      \
		 grep -q 'in progress' ); do				      \
	  echo `date +%I:%M:` "waiting for notarization..." ;		      \
	  sleep 15 ;							      \
	done ;								      \
	echo '' ;							      \
	xcrun altool --notarization-history 0 $(NOTARGS) ;		      \

staple::
	@								      \
  set -e ;								      \
  SRC=../utils/version.h ;						      \
  V=`sed -n 's/[^0-9]*\([0-9]\.[0-9][^. ]*\).*/\1/p' $$SRC` ;		      \
  BASE="xscreensaver-$$V" ;						      \
  OUTDIR="../archive" ;							      \
  DMG="$$OUTDIR/$$BASE.dmg" ;						      \
  set -x ;								      \
  xcrun stapler staple "$$DMG" ;					      \
  xcrun stapler validate "$$DMG"

notarization_history::
	xcrun altool --notarization-history 0 $(NOTARGS)
	@echo 'now do: xcrun altool $(NOTARGS) --notarization-info <UUID>' ;  \
	echo 'and wget the LogFileURL'


# When debugging, sometimes I have to reset the preferences for all
# the savers.  Also I like FPS to be turned on, and them all to be
# pointed at the same image directory.
#
show_prefs::
	@cd build/Debug ;						      \
	for f in *.saver ; do						      \
	  f=`echo "$$f" | sed 's/\..*//'` ;				      \
	  echo "########################## $$f" ;			      \
	  defaults -currentHost read org.jwz.xscreensaver."$$f" 2>&- ;	      \
	done ;								      \
	for f in Apple2 Phosphor updater ; do				      \
	  echo "########################## $$f" ;			      \
	  defaults read org.jwz.xscreensaver."$$f" 2>&- ;		      \
	done

reset_prefs::
	@cd build/Debug ;						      \
	W1='defaults' ;							      \
	W2="$$W1 -currentHost write" ;					      \
	img='~/Pictures/Screensaver' ;					      \
	for f in *.saver ; do						      \
	  name=`echo "$$f" | sed 's/\..*//'` ;				      \
	  echo "########################## $$name" ;			      \
	  domain="org.jwz.xscreensaver" ;				      \
	  dd="$$domain.$$name" ;					      \
	  $$W1 -currentHost delete "$$dd" 2>&- ;			      \
	  $$W2 "$$dd" doFPS -bool true ;				      \
	  if [ -f $$f/Contents/Resources/xscreensaver-text ] ; then	      \
	    $$W2 "$$dd" textMode url ;					      \
	  fi ;								      \
	  if [ -f $$f/Contents/Resources/xscreensaver-getimage-file ] ; then  \
	    $$W2 "$$dd" chooseRandomImages -bool true ;			      \
	    $$W2 "$$dd" grabDesktopImages -bool false ;			      \
	    $$W2 "$$dd" imageDirectory "$$img" ;       			      \
	  fi ;								      \
	  if ( strings "$$f/Contents/MacOS/$$name" |			      \
		grep NSOpenGLContext >/dev/null ) ; then		      \
	    $$W2 "$$dd" multiSample -bool true ;			      \
	  fi ;								      \
	done ;								      \
									      \
	$$W1 delete "$$domain.Apple2" 2>&- ;				      \
	$$W1 delete "$$domain.Phosphor" 2>&- ;				      \
	$$W1 delete "$$domain.updater" 2>&- ;				      \
	$$W1 write  "$$domain.updater" SUScheduledCheckIntervalKey 86400;     \
									      \
	$$W2 "$$domain.BoxFit" grab -bool true ;			      \
	$$W2 "$$domain.FlipFlop" textured -bool true ;			      \
	$$W2 "$$domain.GLSlideshow" titles -bool true ;			      \
	$$W2 "$$domain.Photopile" titles -bool true ;			      \
	$$W2 "$$domain.SkyTentacles" mode cel ;				      \
	$$W2 "$$domain.Sonar" ping					      \
	  '/etc/hosts,$$HOME/.ssh/known_hosts,$$HOME/.ssh/known_hosts2' ;     \
	$$W2 "$$domain.XMatrix" matrixFont small ;			      \
	$$W2 "$$domain.XMatrix" textMode literal ;			      \
	$$W2 "$$domain.XMatrix" textLiteral "MONKEY BUTTER" ;		      \

# defaults -currentHost write org.jwz.xscreensaver.FontGlide debugMetrics -bool true
# defaults -currentHost write org.jwz.xscreensaver.StarWars debug -bool true
# defaults -currentHost write org.jwz.xscreensaver.StarWars textMode file
# defaults -currentHost write org.jwz.xscreensaver.StarWars textFile ~/src/xscreensaver/hacks/glx/zalgo.txt
