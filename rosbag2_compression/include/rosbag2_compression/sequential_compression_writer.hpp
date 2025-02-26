// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROSBAG2_COMPRESSION__SEQUENTIAL_COMPRESSION_WRITER_HPP_
#define ROSBAG2_COMPRESSION__SEQUENTIAL_COMPRESSION_WRITER_HPP_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "rcpputils/thread_safety_annotations.hpp"

#include "rosbag2_cpp/converter.hpp"
#include "rosbag2_cpp/converter_options.hpp"
#include "rosbag2_cpp/serialization_format_converter_factory.hpp"
#include "rosbag2_cpp/serialization_format_converter_factory_interface.hpp"
#include "rosbag2_cpp/writers/sequential_writer.hpp"

#include "rosbag2_storage/metadata_io.hpp"
#include "rosbag2_storage/storage_factory.hpp"
#include "rosbag2_storage/storage_factory_interface.hpp"
#include "rosbag2_storage/storage_options.hpp"
#include "rosbag2_storage/storage_interfaces/base_io_interface.hpp"

#include "rosbag2_compression/compression_options.hpp"

#include "base_compressor_interface.hpp"
#include "compression_factory.hpp"
#include "compression_options.hpp"
#include "visibility_control.hpp"

#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_compression
{

class ROSBAG2_COMPRESSION_PUBLIC SequentialCompressionWriter
  : public rosbag2_cpp::writers::SequentialWriter
{
public:
  explicit SequentialCompressionWriter(
    const rosbag2_compression::CompressionOptions & compression_options =
    rosbag2_compression::CompressionOptions());

  SequentialCompressionWriter(
    const rosbag2_compression::CompressionOptions & compression_options,
    std::unique_ptr<rosbag2_compression::CompressionFactory> compression_factory,
    std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory,
    std::shared_ptr<rosbag2_cpp::SerializationFormatConverterFactoryInterface> converter_factory,
    std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io);

  ~SequentialCompressionWriter() override;

  /**
   * Create a new topic in the underlying storage. Needs to be called for every topic used within
   * a message which is passed to write(...).
   *
   * \param topic_with_type name and type identifier of topic to be created
   * \throws runtime_error if the Writer is not open.
   */
  void create_topic(const rosbag2_storage::TopicMetadata & topic_with_type) override;

  /**
   * Remove a new topic in the underlying storage.
   * If creation of subscription fails remove the topic
   * from the db (more of cleanup)
   *
   * \param topic_with_type name and type identifier of topic to be created
   * \throws runtime_error if the Writer is not open.
   */
  void remove_topic(const rosbag2_storage::TopicMetadata & topic_with_type) override;

  /**
   * If the compression mode is FILE, write a message to a bagfile.
   * If the compression mode is MESSAGE, pushes the message into a queue that will be processed
   * by the compression threads.
   *
   * The topic needs to have been created before writing is possible.
   *
   * \param message to be written to the bagfile
   * \throws runtime_error if the Writer is not open.
   */
  void write(std::shared_ptr<const rosbag2_storage::SerializedBagMessage> message) override;

  /**
   * Opens a new bagfile and prepare it for writing messages. The bagfile must not exist.
   * This must be called before any other function is used.
   *
   * \param storage_options Options to configure the storage
   * \param converter_options options to define in which format incoming messages are stored
   **/
  void open(
    const rosbag2_storage::StorageOptions & storage_options,
    const rosbag2_cpp::ConverterOptions & converter_options) override;

  /**
   * Attempt to compress the last open file and reset the storage and storage factory.
   * This method must be exception safe because it is called by the destructor.
   */
  void close() override;

protected:
  /**
   * Compress a file and update the metadata file path.
   *
   * Note: this may log an error without raising an exception in the case that the input file
   * could not be deleted after compressing. This is an error and should never happen, but given
   * that the desired output is created, execution will not be halted.
   *
   * \param compressor An initialized compression context.
   * \param file_relative_to_bag Relative path of the file to compress, as stored in metadata -
   *   meaning the path is relative to the bag base folder.
   */
  virtual void compress_file(
    BaseCompressorInterface & compressor,
    const std::string & file_relative_to_bag);

  /**
   * Checks if the compression by message option is specified and a compressor exists.
   *
   * If the above conditions are satisfied, compresses the serialized bag message.
   *
   * \param compressor An initialized compression context.
   * \param message The message to compress.
   * \returns The compressed message.
   */
  virtual std::shared_ptr<rosbag2_storage::SerializedBagMessage> compress_message(
    BaseCompressorInterface & compressor,
    std::shared_ptr<const rosbag2_storage::SerializedBagMessage> message);

  /**
   * Initializes the compressor if a compression mode is specified.
   *
   * \throws std::invalid_argument if compression_options isn't supported.
   * \throws rcpputils::IllegalStateException if compressor could not be created
   */
  virtual void setup_compression();

  /**
   * Initializes a number of threads to do file or message compression equal to the
   * value of the compression_threads parameter.
   * \throws rcpputils::IllegalStateException if compressor could not be created
   */
  virtual void setup_compressor_threads();

  /**
   * Signals all compressor threads to stop working and then waits for them to exit.
   */
  virtual void stop_compressor_threads();

private:
  std::shared_ptr<rosbag2_compression::BaseCompressorInterface> compressor_{};
  std::unique_ptr<rosbag2_compression::CompressionFactory> compression_factory_{};
  std::mutex compressor_queue_mutex_;
  std::queue<std::shared_ptr<const rosbag2_storage::SerializedBagMessage>>
  compressor_message_queue_ RCPPUTILS_TSA_GUARDED_BY(compressor_queue_mutex_);
  std::queue<std::string> compressor_file_queue_ RCPPUTILS_TSA_GUARDED_BY(compressor_queue_mutex_);
  std::vector<std::thread> compression_threads_;
  /* *INDENT-OFF* */  // uncrustify doesn't understand the macro + brace initializer
  std::atomic_bool compression_is_running_
    RCPPUTILS_TSA_GUARDED_BY(compressor_queue_mutex_) {false};
  /* *INDENT-ON* */
  std::recursive_mutex storage_mutex_;
  std::condition_variable compressor_condition_;

  rosbag2_compression::CompressionOptions compression_options_{};

  bool should_compress_last_file_{true};

  // Runs a while loop that pulls data from the compression queue until
  // compression_is_running_ is false; should be run in a separate thread
  void compression_thread_fn();

  // Closes the current backed storage and opens the next bagfile.
  void split_bagfile() override;

  // Checks if the current recording bagfile needs to be split and rolled over to a new file.
  bool should_split_bagfile(
    const std::chrono::time_point<std::chrono::high_resolution_clock> & current_time);

  // Prepares the metadata by setting initial values.
  void init_metadata() override;
};
}  // namespace rosbag2_compression
#endif  // ROSBAG2_COMPRESSION__SEQUENTIAL_COMPRESSION_WRITER_HPP_
