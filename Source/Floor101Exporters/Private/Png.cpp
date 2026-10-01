#include "F101/Exporters/Png.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace f101::exporters
{
	namespace
	{
		// Deflate (RFC 1951): lengths 3..258 and distances 1..32768 as symbols plus extra bits.
		constexpr std::array<int, 29> LengthBase{3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83,
			99, 115, 131, 163, 195, 227, 258};
		constexpr std::array<int, 29> LengthExtra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5,
			5, 0};
		constexpr std::array<int, 30> DistanceBase{1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
			1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
		constexpr std::array<int, 30> DistanceExtra{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11,
			11, 12, 12, 13, 13};

		constexpr int MinMatch = 3;
		constexpr int MaxMatch = 258;
		constexpr int WindowSize = 32768;
		constexpr int HashBits = 15;
		constexpr int MaxChain = 48; // candidates tried per position: size against speed

		// Bits least significant first, as deflate packs them.
		class BitWriter
		{
		public:
			explicit BitWriter(std::vector<std::uint8_t>& out)
				: bytes(out)
			{
			}

			void put(std::uint32_t value, int count)
			{
				buffer |= static_cast<std::uint64_t>(value) << filled;
				filled += count;
				while (filled >= 8)
				{
					bytes.push_back(static_cast<std::uint8_t>(buffer & 0xFF));
					buffer >>= 8;
					filled -= 8;
				}
			}

			// A Huffman code: most significant bit first.
			void putCode(std::uint32_t code, int count)
			{
				std::uint32_t reversed = 0;
				for (int bit = 0; bit < count; ++bit)
				{
					reversed |= ((code >> bit) & 1u) << (count - 1 - bit);
				}
				put(reversed, count);
			}

			void flush()
			{
				if (filled > 0)
				{
					bytes.push_back(static_cast<std::uint8_t>(buffer & 0xFF));
				}
				buffer = 0;
				filled = 0;
			}

		private:
			std::vector<std::uint8_t>& bytes;
			std::uint64_t buffer = 0;
			int filled = 0;
		};

		// The fixed literal/length code (RFC 1951, 3.2.6).
		void putLiteralOrLength(BitWriter& bits, int symbol)
		{
			if (symbol <= 143)
			{
				bits.putCode(static_cast<std::uint32_t>(0x30 + symbol), 8);
			}
			else if (symbol <= 255)
			{
				bits.putCode(static_cast<std::uint32_t>(0x190 + symbol - 144), 9);
			}
			else if (symbol <= 279)
			{
				bits.putCode(static_cast<std::uint32_t>(symbol - 256), 7);
			}
			else
			{
				bits.putCode(static_cast<std::uint32_t>(0xC0 + symbol - 280), 8);
			}
		}

		void putMatch(BitWriter& bits, int length, int distance)
		{
			int code = 0;
			while (code + 1 < static_cast<int>(LengthBase.size()) && LengthBase[static_cast<std::size_t>(code) + 1] <= length)
			{
				++code;
			}
			putLiteralOrLength(bits, 257 + code);
			bits.put(static_cast<std::uint32_t>(length - LengthBase[static_cast<std::size_t>(code)]), LengthExtra[static_cast<std::size_t>(code)]);
			int distanceCode = 0;
			while (distanceCode + 1 < static_cast<int>(DistanceBase.size()) &&
				DistanceBase[static_cast<std::size_t>(distanceCode) + 1] <= distance)
			{
				++distanceCode;
			}
			bits.putCode(static_cast<std::uint32_t>(distanceCode), 5);
			bits.put(static_cast<std::uint32_t>(distance - DistanceBase[static_cast<std::size_t>(distanceCode)]),
				DistanceExtra[static_cast<std::size_t>(distanceCode)]);
		}

		std::uint32_t hash3(const std::uint8_t* p)
		{
			const std::uint32_t key = (static_cast<std::uint32_t>(p[0]) << 16) | (static_cast<std::uint32_t>(p[1]) << 8) | p[2];
			return (key * 2654435761u) >> (32 - HashBits);
		}

		void putBigEndian(std::vector<std::uint8_t>& out, std::uint32_t value)
		{
			for (int shift = 24; shift >= 0; shift -= 8)
			{
				out.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
			}
		}

		void putChunk(std::vector<std::uint8_t>& png, const char type[4], const std::vector<std::uint8_t>& data)
		{
			putBigEndian(png, static_cast<std::uint32_t>(data.size()));
			const std::size_t start = png.size();
			png.insert(png.end(), type, type + 4);
			png.insert(png.end(), data.begin(), data.end());
			putBigEndian(png, crc32(png.data() + start, png.size() - start));
		}

		int paeth(int a, int b, int c)
		{
			const int p = a + b - c;
			const int pa = std::abs(p - a);
			const int pb = std::abs(p - b);
			const int pc = std::abs(p - c);
			return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
		}
	}

	std::uint32_t crc32(const std::uint8_t* data, std::size_t size, std::uint32_t crc)
	{
		static const std::array<std::uint32_t, 256> table = []
		{
			std::array<std::uint32_t, 256> entries{};
			for (std::uint32_t n = 0; n < 256; ++n)
			{
				std::uint32_t c = n;
				for (int k = 0; k < 8; ++k)
				{
					c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
				}
				entries[n] = c;
			}
			return entries;
		}();
		std::uint32_t c = crc ^ 0xFFFFFFFFu;
		for (std::size_t i = 0; i < size; ++i)
		{
			c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
		}
		return c ^ 0xFFFFFFFFu;
	}

	std::uint32_t adler32(const std::uint8_t* data, std::size_t size)
	{
		constexpr std::uint32_t Modulus = 65521;
		std::uint32_t a = 1;
		std::uint32_t b = 0;
		for (std::size_t i = 0; i < size; ++i)
		{
			a = (a + data[i]) % Modulus;
			b = (b + a) % Modulus;
		}
		return (b << 16) | a;
	}

	std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& data)
	{
		std::vector<std::uint8_t> out{0x78, 0x01}; // deflate, 32 KB window, no dictionary (check bits: 0x7801 % 31 == 0)
		out.reserve(data.size() / 2 + 64);
		BitWriter bits(out);
		bits.put(1, 1); // the final block
		bits.put(1, 2); // fixed Huffman codes

		const int size = static_cast<int>(data.size());
		std::vector<int> head(std::size_t{1} << HashBits, -1);
		std::vector<int> previous(WindowSize, -1);
		const auto insert = [&](int position)
		{
			if (position + MinMatch <= size)
			{
				const std::uint32_t h = hash3(data.data() + position);
				previous[static_cast<std::size_t>(position % WindowSize)] = head[h];
				head[h] = position;
			}
		};

		int position = 0;
		while (position < size)
		{
			int bestLength = 0;
			int bestDistance = 0;
			if (position + MinMatch <= size)
			{
				int candidate = head[hash3(data.data() + position)];
				const int limit = std::min(MaxMatch, size - position);
				for (int chain = 0; candidate >= 0 && chain < MaxChain && position - candidate <= WindowSize; ++chain)
				{
					int length = 0;
					while (length < limit && data[static_cast<std::size_t>(candidate + length)] == data[static_cast<std::size_t>(position + length)])
					{
						++length;
					}
					if (length > bestLength)
					{
						bestLength = length;
						bestDistance = position - candidate;
						if (length == limit)
						{
							break;
						}
					}
					const int next = previous[static_cast<std::size_t>(candidate % WindowSize)];
					if (next >= candidate)
					{
						break; // a slot reused by a newer position: the chain ends here
					}
					candidate = next;
				}
			}
			if (bestLength >= MinMatch)
			{
				putMatch(bits, bestLength, bestDistance);
				for (int i = 0; i < bestLength; ++i)
				{
					insert(position + i);
				}
				position += bestLength;
			}
			else
			{
				putLiteralOrLength(bits, data[static_cast<std::size_t>(position)]);
				insert(position);
				++position;
			}
		}
		putLiteralOrLength(bits, 256); // end of block
		bits.flush();
		putBigEndian(out, adler32(data.data(), data.size()));
		return out;
	}

	std::vector<std::uint8_t> encodePng(const scene::TextureImage& image)
	{
		const int width = std::max(image.width, 0);
		const int height = std::max(image.height, 0);
		bool opaque = true;
		for (std::size_t i = 3; i < image.rgba.size(); i += 4)
		{
			opaque = opaque && image.rgba[i] == 255;
		}
		const int channels = opaque ? 3 : 4;
		const std::size_t rowBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);

		// Rows of the pixels as stored, then each row filtered with the filter whose output has the smallest sum of
		// absolute values (the usual heuristic of PNG encoders).
		std::vector<std::uint8_t> raw(rowBytes * static_cast<std::size_t>(height));
		for (std::size_t pixel = 0; pixel < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++pixel)
		{
			for (int c = 0; c < channels; ++c)
			{
				raw[pixel * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)] = image.rgba[pixel * 4 + static_cast<std::size_t>(c)];
			}
		}
		std::vector<std::uint8_t> filtered;
		filtered.reserve((rowBytes + 1) * static_cast<std::size_t>(height));
		std::vector<std::uint8_t> candidate(rowBytes);
		std::vector<std::uint8_t> best(rowBytes);
		for (int y = 0; y < height; ++y)
		{
			const std::uint8_t* row = raw.data() + static_cast<std::size_t>(y) * rowBytes;
			const std::uint8_t* above = y > 0 ? row - rowBytes : nullptr;
			std::uint8_t bestFilter = 0;
			long long bestCost = -1;
			for (std::uint8_t filter = 0; filter <= 4; ++filter)
			{
				long long cost = 0;
				for (std::size_t i = 0; i < rowBytes; ++i)
				{
					const int a = i >= static_cast<std::size_t>(channels) ? row[i - static_cast<std::size_t>(channels)] : 0;
					const int b = above ? above[i] : 0;
					const int c = above && i >= static_cast<std::size_t>(channels) ? above[i - static_cast<std::size_t>(channels)] : 0;
					int predictor = 0;
					switch (filter)
					{
					case 1: predictor = a; break;
					case 2: predictor = b; break;
					case 3: predictor = (a + b) / 2; break;
					case 4: predictor = paeth(a, b, c); break;
					default: break;
					}
					const auto value = static_cast<std::uint8_t>((row[i] - predictor) & 0xFF);
					candidate[i] = value;
					cost += value < 128 ? value : 256 - value;
				}
				if (bestCost < 0 || cost < bestCost)
				{
					bestCost = cost;
					bestFilter = filter;
					best.swap(candidate);
				}
			}
			filtered.push_back(bestFilter);
			filtered.insert(filtered.end(), best.begin(), best.end());
		}

		std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
		std::vector<std::uint8_t> header;
		putBigEndian(header, static_cast<std::uint32_t>(width));
		putBigEndian(header, static_cast<std::uint32_t>(height));
		header.push_back(8);                                          // bits per channel
		header.push_back(static_cast<std::uint8_t>(opaque ? 2 : 6)); // RGB or RGBA
		header.push_back(0);                                          // deflate
		header.push_back(0);                                          // adaptive filtering
		header.push_back(0);                                          // not interlaced
		putChunk(png, "IHDR", header);
		putChunk(png, "sRGB", std::vector<std::uint8_t>{0}); // perceptual rendering intent: the pixels are sRGB
		putChunk(png, "IDAT", zlibCompress(filtered));
		putChunk(png, "IEND", {});
		return png;
	}
}
