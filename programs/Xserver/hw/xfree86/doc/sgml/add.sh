#!/bin/sh
#
# $XFree86: xc/programs/Xserver/hw/xfree86/doc/sgml/add.sh,v 1.2 2003/10/18 13:29:34 dawes Exp $
#
name=`basename $1 .sgml`
sgmlfmt -f index $name.sgml | \
	sed -e 's,<title>,<item><htmlurl name=",' \
	    -e 's,</title>," url="'$name.html'"> <htmlurl name="[PDF]" url="'../PDF/$name.pdf'">,' \
	    -e 's,<author>,<!-- ,' \
	    -e 's,</author>, -->,' \
	    -e 's,<date>,<!-- ,' \
	    -e 's,</date>, -->,' >> index.sgml
exit 0
