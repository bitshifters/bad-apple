# Bad Apple - BBC Micro - Teletext

Video codec and player for BBC MODE 7 (aka Teletext)

Coding by [Bitshifters](https://bitshifters.github.io/) - [Kieran](https://github.com/kieranhj) and [Simon](https://github.com/simondotm)

See the demo version on [our site](https://bitshifters.github.io/posts/prods/bs-badapple.html).

**"Bad Apple" - The definitive BBC Micro/Teletext Version**

The Tou Hou Bad Apple [video](https://www.youtube.com/watch?v=G3C-VevI36s) has become a benchmark for pushing retro computing power to the limits. While it has been ported to many other 8-bit platforms, we are now pleased to present the definitive BBC Micro version in glorious Teletext pixel graphics. 

Our version is a full 3m21s of video playback, played back at 25 frames per second in Teletext / MODE 7.

MODE 7 on the BBC Micro used a [Mullard SAA5050](https://en.wikipedia.org/wiki/Mullard_SAA5050) Teletext display/decoder chip which (apart from from subtle implementation differences) is the same [Teletext](https://en.wikipedia.org/wiki/Teletext) chip used in analogue TVs. It is 40x25 characters, supporting 8 primary colours, with support for text characters and basic graphical effects using control codes embedded into each character row. Support for teletext on the BBC Micro was an original requirement of the BBC's specification for the machine due to their own use of broadcast teletext ([Ceefax](https://en.wikipedia.org/wiki/Ceefax)).

The music is a custom VGM chiptune, hand designed by [Inverse Phase](http://www.inversephase.com/) for the BBC Micro's [SN76489](https://en.wikipedia.org/wiki/Texas_Instruments_SN76489). You can support IP's excellent work by [becoming a patron here](https://www.patreon.com/inversephase). 

Intro art by [Horsenburger](http://www.horsenburger.com/), and you can buy awesome stuff from [Horsenburger's store](https://www.tshirtstudio.com/marketplace/horsenburger's-textworks).

The code, music & screens are crammed into a standard 8-bit 2MHz 6502 based BBC Micro's 32Kb RAM, and the video is streamed into memory track by track, after being heavily compressed to fit on one single 400Kb double sided floppy disk image. 


For more information on teletext, take a look at the following sites:
* [TeletextR](http://teletextart.co.uk/) - News & Happenings in the world of Teletext
* [Edit.TF](http://edit.tf/) - A Web Based Teletext Editor
* [Facebook Teletext Group](https://www.facebook.com/groups/TeletextGroup/) - Teletext Community Group
* [Dan Farrimond's Art](http://danfarrimond.co.uk/) - Awesome teletext art
* [Horsenburger's Art](http://www.horsenburger.com/) - Addtional Awesome teletext art

## How it works

### The system
We wanted to create a demo that would work on a standard 32Kb BBC Micro Model B with a single double sided 400Kb disk image. Clearly this would be a challenge given the memory constraints - somehow we'd need to squeeze over 3 minutes of music and video into the available system RAM and disk space.

### The Music
The music is played back on the BBC Micro using raw register updates every 50Hz (using interrupts) to the SN76489 sound chip. Our musician (Inverse Phase) created the music in Deflemask, and exported a 50Hz 150bpm NTSC 3.58Mhz VGM file (which is essentially a raw stream of register data updates).

This VGM file was then processed using [Simon's VGM conversion tool](https://github.com/simondotm/vgm-converter) to transpose the music to the BBC Micros 4Mhz clock speed (so that it sounds correct because the SN76489 generates frequencies that are based on the system clock signal fed into it). 

The same script also outputs the VGM file in a more compact binary format, which takes up a lot less memory and is easier to compress.

Finally this data was compressed using Exomizer, which reduced the filesize to 10.3Kb. Our first compression attempt reduced the file to 19Kb which wasn't enough to fit it into memory with all of the other code. Our musician came up with a cunning plan to remove vibrato on some of the melody tones which did indeed further reduce the memory usage (to 12Kb), but it sounded plainer. After a long evening of trying to come up with a way to do this, analysing the data, looking for patterns, we discovered that if we just compressed the file using a 2Kb compression dictionary window instead of 1Kb we could get the filesize down to 10Kb and keep all of the nice vibrato!

The music is stored in memory compressed, and simply unpacked on demand as we move through the file.

### The intro
We wanted to add an intro sequence AS WELL as all the music & video. This presented a few challenges too, because memory AND disk space was running short. So in the end, we loaded up all of the intro sequence as separately compressed mode 7 screen grabs, stored in the same memory locations as disk streaming buffers that are later used by the video decompression system. This means the intro data is trashed once the video player starts but that's ok.

Horsenburger is something of a whizz kid at teletext art (having been an ACTUAL real life teletext artist back in the day) and he kindly offered to help us with some intro screens which I'm sure you'll agree are pretty awesome.

### The credits
If all of the above wasn't enough (and by the time we'd finished cramming the music the video and the intro in, we were running pretty low on free ram and disk space) we wanted to get some credits in too. These were done about 2 days before the teletext block party event, and we'd put together a quick scrolly effect rendering text using a teletext 'sixels' font.

Inverse Phase spotted the credits and suggested we put some music on there too! MORE stuff to cram in! :)

Well we managed to do that by forcing a quick reload of some data from disk to memory (same memory as the video stream buffer actually), but now but it wasn't scrolling at 50Hz and there was a lot of raster tearing going on - just a side effect of the 6502's speed limits. So one last look at the code, and we managed to hack 10 CPU cycles per character off the update loop, and reduce the scroll area by 2 character lines, and voila - 50Hz smooth scrolling!

### The video playback

[coming soon]


