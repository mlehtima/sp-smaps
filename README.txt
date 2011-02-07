INTRO
=====

sp-smaps is a set of tools for working with SMAPS data (/proc/*/smaps).

sp-smaps contains a SMAPS snapshot utility (`sp_smaps_snapshot') and a set of
post processing tools (`sp_smaps_analyze', etc.).


EXAMPLE
=======

(1) ON TARGET (eg. your desktop machine or a Nokia N900 device)

  Capture smaps information (change directory if no memory card is available):
  # sp_smaps_snapshot > /media/mmc1/smaps.cap

  Copy smaps snapshot to desktop:
  # scp /media/mmc1/smaps.cap <username>@<host>:smaps_data/

(2) ON DESKTOP

  % cd ~/smaps_data

  Create a HTML report from the smaps capture:
  % sp_smaps_analyze smaps.cap

  # View report in a browser:
  % mozilla-firefox smaps.html

For information about other tools and advanced usage,
see the individual manual pages.


CONTACT
=======

Eero Tamminen <eero.tamminen@nokia.com>
