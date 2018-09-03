Network radio application written in C++ to learn low level 
networking programming.

Compile:
```
make
```

Start sender:

```
sox -S "music.flac" -r 44100 -b 16 -e signed-integer -c 2 -t raw - | pv -q -L $((44100*4)) | ./radio-sender -a 239.10.11.12 -n "My radio"
```


Start receiver:

```
./radio-receiver | play -t raw -c 2 -r 44100 -b 16 -e signed-integer --buffer 16384 -
```

Connect to receiver ui interface:

```
telnet localhost 10000
```