01-Aug-2007 Eero Tamminen
- Obsolete information removed

11-Mar-2005 Simo Piiroinen
- typos fixed
- compiler changed
- browser used as reports now in HTML


Basic usage example:


(1) ON TARGET

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
