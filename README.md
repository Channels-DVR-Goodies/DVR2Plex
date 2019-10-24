[![Codacy Badge](https://api.codacy.com/project/badge/Grade/68d3bc77c19b400693c30f07f6fe0fdf)](https://www.codacy.com/manual/paul-chambers/ChanDVR2Plex?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=paul-chambers/ChanDVR2Plex&amp;utm_campaign=Badge_Grade)

# ChanDVR2Plex

**Note:** this tool was written for a Linux environment. It *should* work
fine inside WSL (Windows Services for Linux), but has had little testing
there.

**Caution:** If you've never written a bash script and/or don't know what
'find' can do, this probably isn't the best choice as a first project.
To be useful, this tool needs to be used with other Linux command line
tools, e.g. to hardlink or copy files to their new location. This tool
cannot prevent another tool from blindly replacing an existing
high-quality file with a lower quality one. There are ways to ensure that
doesn't happen (see below), but it's up to you to incorporate them in
your scripts. This tool is sharp, be careful not to cut yourself. It's
not my fault if you don't exercise caution.

## Why does this exist?

I'm a long-time user of Plex, and also use other tools that contribute
content to the Plex library. Both Plex and other tools have a preferred
way to organize that library, and things generally go more smoothly if
everything is using the same naming strategy.

I'm also a fan of the Channels DVR, which has some features that I find
very useful. It has its own 'private' directory where it stores its
recordings, and while it is well-orgainized, it's a little different to
the 'recommended' stucture that Plex, Sonarr, Radarr, et al. prefer.

You can point Plex at the Channels directories holding the recordings,
and Plex will figure things out. But... you can't mess with the contents
of those directories - no renaming or moving files, no organizing them
into 'Season' folders, etc. Channels DVR is expecting things in its 
'private' directory to stay where it put them.

*So,* I wrote a script that hardlinked the recordings Channels DVR made
in its private directory into the 'right place' in my Plex library. A
hard link doesn't use any more disk space, but means I can rename/move
the one in the Plex library without affecting the one in the Channels
DVR 'private' directory. The inverse is also true - the Channels DVR
can delete its file in the 'private' directory, but the other link to
it in the Plex library will remain. Very handy if you want to tell
Channels DVR to 'only keep *n* episodes' (or TiVo/kmttg, for that matter).

*Problem solved, right?* mostly... the main issue is that the world 
hasn't yet settled on how a series is named. For something like
"Marvel's Agents of S.H.I.E.L.D." there are several variations you may
commonly see, like "Marvel's Agents of S.H.I.E.L.D. (2013)", or 
"Marvels Agents of S.H.I.E.L.D" (no single quote, no trailing period),
all the way to "marvels.agents.of.shield". You find that you quickly
build up a number of 'alternate' directories containing episodes of
the same series - often duplicates (particularly if you have more than
one source of recordings, as I do)

Not good. This is the itch this tool scratches.

## OK, but what does it *do?*

In a nutshell, it's a specialized string manipulation tool. You feed it
the name of a media file, it parses out the series, season, episode,
episode title, etc. from the name, and allows you to easily reassemble
a new name from the parts it identified.

That in itself is something you could do with `sed` or `awk`. So why
write this?

The pattern matching is done in a loose/fuzzy way that would be very
hard to do in bash script or general-purpose string manipulation tools.
It uses a different technique than the usual simple character-by-character
string comparison or regular expression methods.
 
See "How does it work?" below, if you're curious about the details.

The 'template' describes the form of the string that this tool should
output. The component parts are substituted in the appropriate place
where you put something like "{episode}". There are a number of these
parameters:

| parameter name | description |
|---             |---          |
| {source}       | The path to the source file, as passed to this tool. |
| {path}         | The 'dirname' part of the source (no trailing slash) |
| {basename}     | The 'basename' of the source (without the extension) |
| {extension}    | The extension. separate so that if what you want to do is convert containers, you can use something like {path}/{basename}.mkv |
| {series}       | The raw name of the series (as extracted from the source |
| {season}       | Always at least two digits, zero-padded |
| {seasonfolder} | If the season is zero, this will be "Specials", otherwise equivalent to "Season {season}"
| {episode}      | Always at least two digits, zero-padded |
| {title}        | The episode title |
| {destseries}   | This is the target folder that the tool determined, by a fuzzy match, is the right destination for the file.<br> More details below. |
| {destination}  | The destination directory for the file. Also used as part of the fuzzy matching | 
| {firstaired}   | the date this episode first aired *(specific to Channels DVR files)* |
| {daterecorded} | the date/time Channels DVR recorded this *(specific to Channels DVR files)* |
| {template}     | it's a parameter too, though you can't use it in a template |

This is only the predefined list of parameters that the parsing will
pre-populate automatically - apart from {destination} and {template},
which need to be defined by the user. They can either be defined on the 
command line, or in a config file - the tool looks for
`/etc/chandvr2plex.conf` and `~/.config/chandvr2plex.conf`, then will
process the config file defined by the `-c` command line option, before
finishing with any command line options. Parameters can be defined
multiple times, the last one wins. So you could, for example, define a
a default {title} as '(unknown)' in a config file, and it would be used
if the file name parsing didn't find an episode title.

You may also define your own parameters in the config file, and use them
in the template. And if the output-building code can't find a parameter
name that matches in its dictionaries, it will also look for an
environment variable with that name (case-sensitive, in this case). So
{HOME} will be replaced by the path to the user's home directory (i.e. '~')

The assumption is that a config file would contain at least the
{destination} and {template} parameters, since those are likely to be
the consistent on a given machine. For example. `/etc/chandvr2plex.conf`
might contain:
```
destination = /home/video/TV
template = "{source}" "{destination}/{destseries?@/}{seasonfolder?@/}{destseries?@ }{season?S@}{episode?E@:-}{title? @}{extension}"
```
So assuming that the source file was 
`/home/Channels/TV/Person of Interest/Person of Interest S02E16 2013-02-21 Relevance 2018-12-30-0000.mpg` 
and a directory existed called `/home/video/TV/Person of Interest (2011)`
then that template would output:
 
 `"/home/Channels/TV/Person of Interest/Person of Interest S02E16 2013-02-21 Relevance 2018-12-30-0000.mpg" "/home/video/TV/Person of Interest (2011)/Season 02/Person of Interest (2011) S02E16 Relevence.mpg"`
 
 but perhaps more impressive is that a source file of `/home/paul/downloads/person.of.interest.2x16.relevence.mpg`
  would also create the same destination of 
  `"/home/video/TV/Person of Interest (2011)/Season 02/Person of Interest (2011) S02E16 Relevence.mpg"`
  
The matching algorithm is not phased by differing case, missing
apostrophes, presence or absence of a year or country (e.g.
"hells_kitchen" will match a directory named "Hell's Kitchen (US)" in
the destination.

This is particularly useful for the worst offenders. For example, I have
a destination folder called "Marvel's Agents of S.H.I.E.L.D. (2013)". The
fuzzy matching can deal with something in the source like
"marvels.agents.of.shield" and still put it in the correct folder.

**Caution:** It's a good practice to include something in the template
that is guaranteed to make the generated name unique, so that it won't
overwrite an existing file in the destination (possibly with a lower
quality version). Since Channel DVR recordings have an .mpg extension,
you'll probably be OK, but better safe than sorry.

### Conditional Expansions
*But wait, what on earth does {episode?E@:-} mean?*

When a parameter isn't defined, it expands to nothing. Which is all well
and good, except if there's some surrounding characters that need to
disappear too. {episode?E@:-} means 'if {episode} is defined, output 'E'
followed by the contents of {episode}, otherwise output just '-'.
Similarly {seasonfolder} would normally be seen in a template written
as {seasonfolder?@/} so that the path separator is only included if 
{seasonfolder} is defined.

This is akin to the trinary operator in C, if you're a programmer -
up the the '?' is the thing to test, after the '?' and before the ':'
or '}' is the string to output if it is defined ('true'), between the
':' and '}' is the string to output if it isn't defined ('false').
Where an '@' appears, insert the value of the parameter.

## How does it work?

The tool uses modified hashing to do comparisons. The hashing is
modified by mapping each character through a mapping table first, so
that particular characters can be mapped to another, or ignored
completely. For example, upper case characters are mapped to lower case,
so "UPPER" has the same hash as "upper" or "UpPeR"

This fixes all-lowercase series for example, or random "Of"/"of" confusion
(or "MythBusters" vs. "Mythbusters")

Some characters are often dropped, like apostrophes or the trailing
period of an acronym, like S.W.A.T., so those are ignored. "Marvel's"
matches "marvels", "swat" or "S.W.A.T" matches "S.W.A.T."

All digits are mapped to '0' (though only for the pattern matching), so
we have a constant hash for patterns like S02E10, S01E05, etc. This
allows us to easily find the several patterns we're looking for, very
efficiently. Those patterns are mostly season/episode patterns: SnnEnn,
SnnnnEnn, nXnn, nnXnn and a few less-common variations. We also identify
nnnn-nn-nn as the pattern for the 'first aired' date for Channels DVR
recordings, along with nnnn-nn-nn-nnnn for the date recorded.

There are in fact two character maps used for hashing. One for finding
patterns, and another for looking up parameters. The main difference is
that the parameter mapping doesn't map all digits to zero.

Internally, there are three 'dictionaries' used for searching, each is 
a simple key-value list, using the hash as the key. The 'main'
dictionary contains all the parameters that will remain the same for
the entire run. This is the dictionary populated from the config files
and command line options. There is a 'file' dictionary, which contains
the parsed value for the last source parsed. This is discarded and
rebuilt for each source, so per-file values don't carry over from one
source to the next.

The third dictionary is the 'series' one, which is populated with hashes
of the directory names that it finds by doing a scan of the {destination}
directory. The assumption is that these are essentially the canonical
names/destinations for the known TV series. For a given series directory,
there may be more than one dictionary entry, as the hash without the
year or country is included, as well as with it included. So the hash
for "Person of Interest" is stored as well as "Person of Interest (2011)",
both pointing to the target "Person of Interest (2011)" directory.
Thus either form will match, and point to the right destination directory.
This is the mechanism behind the {destfolder} parameter.
