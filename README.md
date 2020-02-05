[![Codacy Badge](https://api.codacy.com/project/badge/Grade/68d3bc77c19b400693c30f07f6fe0fdf)](https://www.codacy.com/manual/paul-chambers/DVR2Plex?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=paul-chambers/DVR2Plex&amp;utm_campaign=Badge_Grade)

[Full Documentation](https://paul-chambers.github.io/DVR2Plex/)

# DVR2Plex

**Caution:** If you are new to the linux command line, and/or are unfamiliar
with common linux tools like 'find', I wouldn't recommend this as a good first
project because of the danger of overwriting existing files.

This tool uses some fancy text processing techniques to reformat filename
into another one. To be useful, this tool needs to be used with other Linux 
command line tools, e.g. to copy (or hardlink) files to their new location.

Since DVR2Plex isn't actually doing the copy/hardlinking itself, it
cannot prevent the tool doing the copy from blindly overwriting an existing
high-quality file with a lower quality one. Thus it's best to have your
template generate a destination filename that won't overwrite any existing
files.

You must accept responsibility for your configuration and use of this tool,
and accept that data loss is a possibility. Be careful when using this tool.

**Note:** this tool was written for a Linux environment. It *should* work
fine inside WSL (Windows Services for Linux), but has had little testing
there.

## Why does this exist?

I'm a long-time user of [Plex](https://plex.tv/), and use related tools to
supply content for the Plex content library. Plex has a preferred way it
likes to see the library organized, and things generally go more smoothly
if everything uses the same organization and naming strategy.

I'm also a fan of the [Channels DVR](https://getchannels.com/dvr-server/), 
which is well implemented and has some features that I find particularly
useful. It keeps its recordins in a private directory, and while it is
well-orgainized, it's in a way that's a little different to the structure
that Plex prefers. More importantly, Channels DVR 'owns' the files in
that folder, and other software should respect that, and not 'pull the rug'
out from under Channels DVR by messing with those files behind its back.

While you could point Plex at the Channels directories holding the
recordings, and Plex will figure things out. However, it should treat
those directory contents as read-only, otherwise Plex will be altering
files that Channels owns.
 
I initially wrote a shell script that hardlinked the recordings Channels
DVR made in its private directory into the 'right place' in my Plex
library. A 'hard link' doesn't use any more disk space, and has the
positive attribute that the hard link in the Plex Library can be moved
and/or renamed without affecting the one in the Channels DVR 'private'
directory. The inverse is also true - the Channels DVR can delete its
file in the 'private' directory, without affecting the other link to it
in the Plex library, so it will remain. This is very handy when used
with the 'only keep *n* episodes' in Channels DVR (or
[kmttg](https://sourceforge.net/projects/kmttg), for that matter).

*Problem solved, right?* Well, mostly...

The biggest issue with such a simple approach is that the world 
hasn't yet settled on how a series is named, and possibly never will.
For example, a series like "Marvel's Agents of S.H.I.E.L.D." there
are a number of variations on that title that you'll see in the wild.
Variations like "Marvel's Agents of S.H.I.E.L.D. (2013)", or 
"Marvels Agents of S.H.I.E.L.D" (no single quote, no trailing period),
all the way to "marvels.agents.of.shield".
 
If that isn't accounted for, then differently-named directories will
accumulate, containing episodes of the same series, often duplicates.
This only gets worse as the number of content sources increases.

Not good. This is the itch this tool scratches.

## OK, but what does it *do?*

In a nutshell, it's a specialized string manipulation tool. You feed it
the name of a media file, it parses out the series, season, episode,
episode title, etc. from the name, and provides a 'template' system 
that allows you to easily reassemble a new name for the destination
from the parts it extracted from the source filename.

*That sounds like something you could do with `sed` or `awk`. So why
write this?*

The pattern matching is done in a loose/fuzzy way that would be 
impractical to do in bash script or command-line string manipulation
tools.

It uses a fundamentally different technique - character-mapped hashing
- than the usual simple character-by-character string comparison or
regular expression methods.
 
See "How does it work?" below, if you're curious about the details.

The 'template' describes the form of the string that this tool should
output. The component parts are substituted in the appropriate place
where you put something like "{episode}". There are quite a number of
these parameters:

| Parameter Name | Description                                                                     |
|---             |---                                                                              |
| {source}       | The path to the source file, as passed to this tool.                            |
| {path}         | The 'dirname' part of the source (no trailing slash)                            |
| {basename}     | The 'basename' of the source (minus the extension)                              |
| {extension}    | The extension. Separated so that if what you want to do is convert containers, you can write something like {path}/{basename}.mkv as the destination in the template |
| {series}       | The raw name of the series (as extracted from the source)                       |
| {season}       | Always at least two digits, zero-padded                                         |
| {seasonfolder} | If the season is zero, this will be "Specials", otherwise "Season {season}"     |
| {episode}      | Always at least two digits, zero-padded                                         |
| {title}        | The episode title                                                               |
| {destseries}   | This is the target folder that the tool determined (by fuzzy match) is the right destination for the file.<br> More details below. |
| {destination}  | The destination directory for the file. Also scanned as part of the fuzzy matching | 
| {firstaired}   | The date this episode first aired *(specific to Channels DVR files)*            |
| {daterecorded} | The date/time Channels DVR recorded this *(specific to Channels DVR files)*     |
| {template}     | It's a parameter too (though you can't use it in a template, obviously)         |

This is only the predefined list of parameters that the parsing will
pre-populate automatically - except for {destination} and {template},
which need to be defined by the user. They can either be defined on the 
command line, or in a config file - the tool looks for
`/etc/DVR2Plex.conf` and `~/.config/DVR2Plex.conf`, then will
process the config file defined by the `-c` command line option, before
finishing with any command line options. Parameters can be defined
multiple times, the last one wins. So you could, for example, define a
a default {title} as '(unknown)' in a config file, and it would be used
if the file name parsing didn't find an episode title.

You may also define your own parameters in the config file, and use them
in the template. And if the output-building code can't find a parameter
name that matches in its dictionaries, it will also look for an
environment variable with that name (case-sensitive, in this case). So
{HOME} will be replaced by the path to the user's home directory (i.e.
 the equivalent of '~')

The assumption is that a config file would contain at least the
{destination} and {template} parameters, since those are likely to be
the consistent on a given machine. For example. `/etc/DVR2Plex.conf`
might contain:
```
destination = /home/video/TV
template = "{source}" "{destination}/{destseries?@/}{seasonfolder?@/}{destseries?@ }{season?S@}{episode?E@:-}{title? @}{extension}"
```
So assuming that the source file was 
`/home/Channels/TV/Person of Interest/Person of Interest S02E16 2013-02-21 Relevance 2018-12-30-0000.mpg` 
and a directory existed called `/home/video/TV/Person of Interest (2011)`
then that template would output:
 
 `"/home/video/TV/Person of Interest/Person of Interest S02E16 2013-02-21 Relevance 2018-12-30-0000.mpg" "/home/video/TV/Person of Interest (2011)/Season 02/Person of Interest (2011) S02E16 Relevence.mpg"`
 
 but perhaps more impressive is that a source file of `/home/paul/downloads/person.of.interest.2x16.relevence.mpg`
  would also create the same destination of 
  `"/home/video/TV/Person of Interest (2011)/Season 02/Person of Interest (2011) S02E16 Relevence.mpg"`

**Caution:** It's a good practice to include something in the template
that is guaranteed to make the generated name unique, so that it won't
overwrite an existing file in the destination (portentially a lower
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

DVR2Plex first builds up a list of hashes for the directories
found in the {destination} directory.

The matching algorithm is not phased by differing case, missing
apostrophes, presence or absence of a year or country (e.g.
"hells_kitchen" will match a directory named "Hell's Kitchen (US)" in
the destination.

This is particularly useful for the worst offenders. For example, if you
have a destination folder called "Marvel's Agents of S.H.I.E.L.D. (2013)".
The fuzzy matching can deal with something in the source like
"marvels.agents.of.shield" and still put it in the correct folder.

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
