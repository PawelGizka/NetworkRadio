to start sender:

```
sox -S "music.flac" -r 44100 -b 16 -e signed-integer -c 2 -t raw - | pv -q -L $((44100*4)) | ./Sender -a 239.10.11.12 -n "My radio"
```


to start receiver:

```
./Receiver | play -t raw -c 2 -r 44100 -b 16 -e signed-integer --buffer 1024 -
```

It is far better to start receiver with low `play` buffer value and with high program buffer value 