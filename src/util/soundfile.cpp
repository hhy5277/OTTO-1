#include "util/soundfile.hpp"
#include "services/log_manager.hpp"

namespace otto::util {

  using Chunk = ByteFile::Chunk;
  using Position = SoundFile::Position;

  struct Header : Chunk {
    Header() = default;
    Header(Chunk& o) : Chunk(o) {}

    bytes<4> format;
    std::vector<std::unique_ptr<Chunk>> chunks;

    void read_fields(ByteFile& file) override {
      file.read_bytes(format);
      file.for_chunks_in_range(offset + 12, offset + 8 + size.as_u(),
        [&](Chunk& c) {
          chunks.emplace_back(new Chunk(c));
        });
    }

    void write_fields(ByteFile& file) override {
      file.write_bytes(format);
      for (auto&& c : chunks) {
        c->write(file);
      }
    }
  };

  struct WAVE_fmt : Chunk {
    WAVE_fmt() : Chunk("fmt ") {}
    WAVE_fmt(Chunk& o) : Chunk(o) {}

    bytes<2> audioFormat = 3;
    bytes<2> numChannels;
    bytes<4> sampleRate;
    bytes<4> byteRate;
    bytes<2> blockAlign;
    bytes<2> bitsPerSample;

    void read_fields(ByteFile& file) override {
      auto& sf = (SoundFile&)file;

      file.read_bytes(audioFormat);
      file.read_bytes(numChannels);
      file.read_bytes(sampleRate);
      file.read_bytes(byteRate);
      file.read_bytes(blockAlign);
      file.read_bytes(bitsPerSample);

      if (audioFormat.as_u() != 3) {
        throw "Unsupported audio format. \
               Currently only float is supported";
      }

      sf.info.channels = numChannels.as_u();
      sf.info.samplerate = sampleRate.as_u();

      if (bitsPerSample.as_u() != 32) {
        throw "Unsupported sample size. \
              Currently only 32bit float is supported";
      }
    }

    void write_fields(ByteFile& file) override {
      auto& sf = (SoundFile&)file;
      audioFormat.as_u() = 3;
      numChannels.as_u() = sf.info.channels;
      sampleRate.as_u() = sf.info.samplerate;
      bitsPerSample.as_u() = sf.sample_size * 8;
      byteRate.as_u() =
        sampleRate.as_u() * numChannels.as_u() * bitsPerSample.as_u() / 8;
      blockAlign.as_u() = numChannels.as_u() * bitsPerSample.as_u() / 8;

      file.write_bytes(audioFormat);
      file.write_bytes(numChannels);
      file.write_bytes(sampleRate);
      file.write_bytes(byteRate);
      file.write_bytes(blockAlign);
      file.write_bytes(bitsPerSample);
    }
  };

  struct WAVE_data : Chunk {
    WAVE_data() : Chunk("data") {}
    WAVE_data(Chunk& c) : Chunk(c) {}

    void read_fields(ByteFile& file) override {
      auto& sf = (SoundFile&)file;
      sf.audioOffset = offset + 8;
    }
    void write_fields(ByteFile& file) override {
      auto& sf = (SoundFile&)file;
      sf.audioOffset = offset + 8;
    }
  };

  /*
   * SoundFile Implementation
   */

  SoundFile::SoundFile() {}

  void SoundFile::read_file() {
    ByteFile::seek(0);
    Header header;
    header.read(*this);

    if (header.id == "RIFF" && header.format == "WAVE") {
      info.type = Info::Type::WAVE;
    } else if (header.id == "FORM" && header.format == "AIFF") {
      info.type = Info::Type::AIFF;
    } else if (header.id.as_u() == 0) {
      create_file();
    } else {
      throw Error::UnrecognizedFileType.append(
        fmt::format("Got {} while reading file {}", header.id.str(), path.c_str()));
    }

    switch (info.type) {
    case Info::Type::WAVE:
      LOG_F(INFO, "Reading Wave file: {}", path);
      LOG_F(INFO, "-------------------");

      for (auto&& chunk : header.chunks) {
        if (chunk->id == "fmt ")
          chunk = std::make_unique<WAVE_fmt>(*chunk);
        if (chunk->id == "data")
          chunk = std::make_unique<WAVE_data>(*chunk);
        replace_custom_chunk(chunk);

        // re-read the chunk as the new type
        ByteFile::seek(chunk->beginning());
        chunk->read(*this);

        LOG_F(INFO, " Chunk:  {}", std::string((char*)chunk->id.data, 4));
        LOG_F(INFO, " Offset: {}", chunk->offset);
        LOG_F(INFO, " Size:   {}", chunk->size.as_u());
        LOG_F(INFO, "-------------------");
      } break;

      LOG_F(INFO, "Done reading file! ");
      LOG_F(INFO, "-------------------");
    case Info::Type::AIFF:
      throw "Unsupported file type. Currently only wav is supported";
    }
    seek(0);
  }

  void SoundFile::write_file() {
    ByteFile::seek(0);
    Header header;

    switch (info.type) {
    case Info::Type::WAVE:
      LOG_F(INFO, "Writing Wave file: {}", path);
      LOG_F(INFO, "-------------------");

      header.id = "RIFF";
      header.format = "WAVE";
      header.chunks.push_back(std::make_unique<WAVE_fmt>());
      add_custom_chunks(header.chunks);
      header.chunks.push_back(std::make_unique<WAVE_data>());
      header.chunks.back()->size = ByteFile::size() - audioOffset;
      header.write(*this);

      LOG_F(INFO, "Wrote {} chunks", header.chunks.size());
      LOG_IF_F(ERROR, !fstream.good(), "fstream errored");
      LOG_F(INFO, "-------------------");
      break;
    case Info::Type::AIFF:
      throw "Unsupported type. Currently only wav is supported";
    }
  }

  Position SoundFile::seek(Position p) {
    return (ByteFile::seek(audioOffset + p * sample_size)
      - audioOffset) / sample_size;
  }

  Position SoundFile::position() {
    Position r = (ByteFile::position() - audioOffset) / sample_size;
    if (r < 0) {
      seek(0);
      return 0;
    }
    return r;
  }

  Position SoundFile::length() {
    return (ByteFile::size() - audioOffset) / sample_size;
  }
}
