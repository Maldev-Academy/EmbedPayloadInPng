## EmbedPayloadInPng

Embed a payload within a PNG file by splitting the payload across multiple `IDAT` sections. Each section is encrypted individually using its own 16-byte key with the RC4 encryption algorithm.

### Quick Links

[Maldev Academy Home](https://maldevacademy.com?ref=gh)
  
[Maldev Academy Syllabus](https://maldevacademy.com/syllabus?ref=gh)

[Maldev Academy Pricing](https://maldevacademy.com/pricing?ref=gh)


## Implementation

This repository consists of two implementations:

* `EmbedPayloadInPng.py` - Python script to embed an input payload to a specified PNG file.
* `FetchPayloadFromPng` - Extract the payload from `EmbedPayloadInPng.py`'s outputted PNG file, and decrypt it using the [ExtractDecryptedPayload](https://github.com/Maldev-Academy/EmbedPayloadInPng/blob/main/FetchPayloadFromPng/main.c#L92) function.


## Usage

1. Use `EmbedPayloadInPng.py` to create the embedded payload PNG file:

<div style="text-align: center;"><img width="1200px" alt="image" src="https://github.com/user-attachments/assets/a92e998c-c45c-451f-bb5e-5368e5fc3357"></div>

2. Copy the `MARKED_IDAT_HASH` macro definition outputted by `EmbedPayloadInPng.py` and replace it with the existing one in the `FetchPayloadFromPng` project [here](https://github.com/Maldev-Academy/EmbedPayloadInPng/blob/main/FetchPayloadFromPng/main.c#L23C1-L24C1).



## Embedded PNG File Structure

As mentioned earlier, `EmbedPayloadInPng.py` is responsible for embedding the payload file within a PNG one. Below is the structure of a payload-embedded PNG file.

<div style="text-align: center;"><img width="800px" alt="image" src="https://github.com/user-attachments/assets/a2e76c15-e954-464c-8233-0a010292b94f"></div>

Since the maximum size of an `IDAT` section is 8192 bytes, our payload is chunked to multiple `IDAT` sections. Each section has a size equivalent to [(8192 - 16 [RC4 key length])](https://github.com/Maldev-Academy/EmbedPayloadInPng/blob/main/EmbedPayloadInPng.py#L124). Furthermore, The last `IDAT` section will contain the remaining bytes of the payload.


</br>

The following images explain `EmbedPayloadInPng.py`'s output and compare it to the structure of the created PNG file:  

* The output PNG file sections. 
<div style="text-align: center;"><img width="800px" alt="image" src="https://github.com/user-attachments/assets/2419953e-b940-4ee3-899d-80e1f815a685"></div>

</br>

* The random IDAT section, which is created to mark the start of our payload. The CRC hash of this section is used in [our C code](https://github.com/Maldev-Academy/EmbedPayloadInPng/blob/main/FetchPayloadFromPng/main.c#L23C9-L23C26) to [identify](https://github.com/Maldev-Academy/EmbedPayloadInPng/blob/main/FetchPayloadFromPng/main.c#L138) the start of our payload in the PNG file.
<div style="text-align: center;"><img width="1200px" alt="image" src="https://github.com/user-attachments/assets/2030f3f4-7cba-43c8-9e8c-a82af01815c5"></div>

</br>

* The first payload `IDAT` section, following our random section (in blue). This image also demonstrates the position of the CRC hash and the size of the randomized IDAT section beforehand (in yellow).   
<div style="text-align: center;"><img width="1200px" alt="image" src="https://github.com/user-attachments/assets/254cde8b-8db6-44f3-ae87-f9707d6d53e8"></div>

</br>

* The CRC hash of the first payload IDAT section, which is located at the end of the section following the encrypted first chunk of our payload.
<div style="text-align: center;"><img width="1200px" alt="image" src="https://github.com/user-attachments/assets/a4305214-d5bd-4666-91fe-f1c853366ec5"></div>

</br>

* The start of the second payload IDAT section.
<div style="text-align: center;"><img width="1200px" alt="image" src="https://github.com/user-attachments/assets/0a6cfe32-534a-447f-8a6f-fe7544cb0348"></div>



