// Copyright 2018, Bosch Software Innovations GmbH.
// Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#ifndef ROSBAG2_CPP__READER_HPP_
#define ROSBAG2_CPP__READER_HPP_

#include <memory>
#include <string>
#include <vector>

#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"

#include "rosbag2_cpp/bag_events.hpp"
#include "rosbag2_cpp/converter_options.hpp"
#include "rosbag2_cpp/readers/sequential_reader.hpp"
#include "rosbag2_cpp/visibility_control.hpp"

#include "rosbag2_storage/bag_metadata.hpp"
#include "rosbag2_storage/serialized_bag_message.hpp"
#include "rosbag2_storage/storage_filter.hpp"
#include "rosbag2_storage/storage_options.hpp"
#include "rosbag2_storage/topic_metadata.hpp"

// This is necessary because of using stl types here. It is completely safe, because
// a) the member is not accessible from the outside
// b) there are no inline functions.
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_cpp
{
namespace reader_interfaces
{
class BaseReaderInterface;
}  // namespace reader_interfaces

/**
 * The Reader allows opening and reading messages of a bag.
 */
class ROSBAG2_CPP_PUBLIC Reader
{
public:
  explicit Reader(
    std::unique_ptr<reader_interfaces::BaseReaderInterface> reader_impl =
    std::make_unique<readers::SequentialReader>());

  ~Reader();

  /**
   * Opens an existing bagfile and prepare it for reading messages.
   * The bagfile must exist.
   * This must be called before any other function is used.
   *
   * \note This will open URI with the default storage options
   * * using default storage backend
   * * using no converter options, storing messages with the incoming serialization format
   * \sa rmw_get_serialization_format.
   * For specifications, please see \sa open, which let's you specify
   * more storage and converter options.
   *
   * \param storage_uri URI of the storage to open.
   **/
  void open(const std::string & uri);

  /**
   * Throws if file could not be opened.
   * This must be called before any other function is used.
   * The rosbag is automatically closed on destruction.
   *
   * If the `output_serialization_format` within the `converter_options` is not the same as the
   * format of the underlying stored data, a converter will be used to automatically convert the
   * data to the specified output format.
   * Throws if the converter plugin does not exist.
   *
   * \param storage_options Options to configure the storage
   * \param converter_options Options for specifying the output data format
   */
  void open(
    const rosbag2_storage::StorageOptions & storage_options,
    const ConverterOptions & converter_options = ConverterOptions());

  /**
   * Closing the reader instance.
   */
  void close();

  /**
   * Set the read order for continued iteration of messages, without changing the current
   * read head timestamp.
   *
   * \param read_order Sorting criterion and direction to read messages in
   * \throws runtime_error if the Reader is not open.
   * \return true if the requested read order has been successfully set.
   * \note Calling set_read_order(order) concurrently with has_next(), seek(t), has_next_file()
   * or load_next_file() will cause undefined behavior.
   */
  bool set_read_order(const rosbag2_storage::ReadOrder & read_order);

  /**
   * Ask whether the underlying bagfile contains at least one more message.
   *
   * \return true if storage contains at least one more message
   * \throws runtime_error if the Reader is not open.
   */
  bool has_next();

  /**
   * Read next message from storage. Will throw if no more messages are available.
   * The message will be serialized in the format given to `open`.
   *
   * Expected usage:
   * if (writer.has_next()) message = writer.read_next();
   *
   * \return next message in serialized form
   * \throws runtime_error if the Reader is not open.
   */
  std::shared_ptr<rosbag2_storage::SerializedBagMessage> read_next();

  /**
   * Read next message from storage. Will throw if no more messages are available.
   * The message will be serialized in the format given to `open`.
   *
   * Expected usage:
   * if (writer.has_next()) message = writer.read_next();
   *
   * \return next message in non-serialized form
   * \throws runtime_error if the Reader is not open.
   */
  template<class MessageT>
  MessageT read_next()
  {
    MessageT msg;
    auto bag_message = read_next();
    rclcpp::SerializedMessage extracted_serialized_msg(*bag_message->serialized_data);
    rclcpp::Serialization<MessageT> serialization;
    serialization.deserialize_message(&extracted_serialized_msg, &msg);

    return msg;
  }

  /**
    * Ask bagfile for its full metadata.
    *
    * \return a const reference to a BagMetadata owned by the Reader
    * \throws runtime_error if the Reader is not open.
    */
  const rosbag2_storage::BagMetadata & get_metadata() const;

  /**
   * Ask bagfile for all topics (including their type identifier) that were recorded.
   *
   * \return vector of topics with topic name and type as std::string
   * \throws runtime_error if the Reader is not open.
   */
  std::vector<rosbag2_storage::TopicMetadata> get_all_topics_and_types() const;

  /**
   * Set filters to adhere to during reading.
   *
   * \param storage_filter Filter to apply to reading
   * \throws runtime_error if the Reader is not open.
   */
  void set_filter(const rosbag2_storage::StorageFilter & storage_filter);

  /**
   * Reset all filters for reading.
   */
  void reset_filter();

  /**
   * Skip to a specific timestamp for reading.
   */
  void seek(const rcutils_time_point_value_t & timestamp);

  reader_interfaces::BaseReaderInterface & get_implementation_handle() const
  {
    return *reader_impl_;
  }

  /**
   * \brief Add callbacks for events that may occur during bag reading.
   * \param callbacks the structure containing the callback to add for each event.
   */
  void add_event_callbacks(bag_events::ReaderEventCallbacks & callbacks);

private:
  std::unique_ptr<reader_interfaces::BaseReaderInterface> reader_impl_;
};

}  // namespace rosbag2_cpp

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // ROSBAG2_CPP__READER_HPP_
