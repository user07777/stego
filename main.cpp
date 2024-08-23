#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <iomanip>
#include <shlobj.h>

std::pair<size_t, std::vector<unsigned char>> getBin(const std::string& path) {
	HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	std::pair<size_t, std::vector<unsigned char>> data = std::make_pair<size_t, std::vector<unsigned char>>(0, {});

	if (hFile == INVALID_HANDLE_VALUE) {
		std::cerr << "can't read " << path << std::endl;
		return data;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize)) {
		std::cerr << "Can't read" << std::endl;
		CloseHandle(hFile);
		return data;
	}

	std::vector<unsigned char> buffer(fileSize.QuadPart);

	DWORD bytesRead;
	if (!ReadFile(hFile, buffer.data(), static_cast<DWORD>(fileSize.QuadPart), &bytesRead, NULL) || bytesRead != fileSize.QuadPart) {
		std::cerr << "can't read" << std::endl;
		CloseHandle(hFile);
		return data;
	}

	CloseHandle(hFile);

	size_t size = buffer.size();
	std::cout << " Size: " << std::setfill('0') << size / 1024 << "kb" << "\n";

	data.first = size;
	data.second = buffer;
	return data;
}

void save(const std::string& path, LPVOID data, DWORD size) {
	char documentsPath[MAX_PATH];
	if (SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, documentsPath) != S_OK) {
		std::cerr << "Can't find \\documents" << std::endl;
		return;
	}

	std::string estegoPath = std::string(documentsPath) + "\\estego";
	CreateDirectoryA(estegoPath.c_str(), NULL);

	std::string filename = path.substr(path.find_last_of("\\/") + 1);
	std::string stego = estegoPath + "\\" + filename;

	HANDLE hFile = CreateFileA(stego.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		std::cerr << "Can't write " << stego << std::endl;
		return;
	}

	DWORD bytesWritten;
	if (!WriteFile(hFile, data, size, &bytesWritten, NULL) || bytesWritten != size) {
		std::cerr << "Can't write " << std::endl;
	}
	else {
		std::cout << "saved ;) " << stego << '\n';
	}

	CloseHandle(hFile);
}

int main() {
	std::string path = "C:\\Users\\< user >\\Documents\\estego\\estego.exe";
	std::string filePath = "C:\\Users\\< user >\\Documents\\reversing-for-everyone.pdf";
	std::string type = "get";

	if (type == "set") {
		std::pair<size_t, std::vector<unsigned char>> fileData = getBin(filePath);

		HANDLE file = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE) {
			std::cerr << "Can't open " << path << std::endl;
			return 1;
		}

		DWORD fileSize = GetFileSize(file, NULL);
		LPVOID data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize);

		std::cout << " Size: " << std::setfill('0') << fileSize / 1024 << "kb" << "\n";

		if (data == NULL) {
			std::cerr << "HeapAlloc err" << std::endl;
			CloseHandle(file);
			return 1;
		}

		DWORD bytesRead;
		if (!ReadFile(file, data, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
			std::cerr << "Can't read " << path << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}

		PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(data);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
			std::cerr << "DOS header" << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}

		PIMAGE_NT_HEADERS pntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(data) + dosHeader->e_lfanew);
		if (pntHeaders->Signature != IMAGE_NT_SIGNATURE) {
			std::cerr << "NT header" << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}
		PIMAGE_SECTION_HEADER sectionHeaders = IMAGE_FIRST_SECTION(pntHeaders);
		size_t sects = pntHeaders->FileHeader.NumberOfSections;

		DWORD fsz = pntHeaders->OptionalHeader.SizeOfImage + fileData.first;

		LPVOID fdata = HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, data, fsz);
		if (fdata == NULL) {
			std::cerr << "HeapReAlloc err" << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}

		data = fdata;
		pntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(data) + dosHeader->e_lfanew);
		sectionHeaders = IMAGE_FIRST_SECTION(pntHeaders);

		void* ptrToEstego = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(data) + sectionHeaders[static_cast<unsigned long long>(sects) - 1].PointerToRawData + sectionHeaders[static_cast<unsigned long long>(sects) - 1].SizeOfRawData);

		memcpy(ptrToEstego, fileData.second.data(), fileData.first);
		pntHeaders->OptionalHeader.SizeOfImage += fileData.first;

		std::cout << std::hex << ptrToEstego << "\n";
		save(path, data, fsz);
		HeapFree(GetProcessHeap(), 0, data);
		CloseHandle(file);
	}

	else {
		auto out = path + "_extracted";

		HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE) {
			std::cerr << "CreateFileA " << path << std::endl;
			return 1;
		}

		DWORD fileSize = GetFileSize(file, NULL);
		LPVOID data = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, fileSize);

		if (data == NULL) {
			std::cerr << "HeapAlloc" << std::endl;
			CloseHandle(file);
			return 1;
		}

		DWORD bytesRead;
		if (!ReadFile(file, data, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
			std::cerr << "Can't read " << path << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}

		PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(data);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
			std::cerr << "DOS header" << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}

		PIMAGE_NT_HEADERS pntHeaders = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(data) + dosHeader->e_lfanew);
		if (pntHeaders->Signature != IMAGE_NT_SIGNATURE) {
			std::cerr << "NT header" << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}

		PIMAGE_SECTION_HEADER sectionHeaders = IMAGE_FIRST_SECTION(pntHeaders);

		size_t sects = pntHeaders->FileHeader.NumberOfSections;

		void* ptrToEstego = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(data) + sectionHeaders[static_cast<unsigned long long>(sects) - 1].PointerToRawData + sectionHeaders[static_cast<unsigned long long>(sects) - 1].SizeOfRawData);

		DWORD imageSize = static_cast<DWORD>(fileSize - (reinterpret_cast<uintptr_t>(ptrToEstego) - reinterpret_cast<uintptr_t>(data)));

		out += "_" + std::to_string(reinterpret_cast<uintptr_t>(ptrToEstego));

		HANDLE hFile = CreateFileA(out.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			std::cerr << "Can't write" << std::endl;
			HeapFree(GetProcessHeap(), 0, data);
			CloseHandle(file);
			return 1;
		}

		DWORD bytesWritten;

		if (!WriteFile(hFile, ptrToEstego, imageSize, &bytesWritten, NULL) || bytesWritten != imageSize) {
			std::cerr << "Can't write" << std::endl;
		}

		else {
			std::cout << "extracted ;)" << '\n';
		}

		CloseHandle(hFile);
		HeapFree(GetProcessHeap(), 0, data);
		CloseHandle(file);
	}

	return 0;
}
