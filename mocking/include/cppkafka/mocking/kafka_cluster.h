#ifndef CPPKAFKA_MOCKING_KAFKA_CLUSTER_H
#define CPPKAFKA_MOCKING_KAFKA_CLUSTER_H

#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <functional>
#include <stdexcept>
#include <cppkafka/mocking/kafka_topic_mock.h>
#include <cppkafka/mocking/kafka_message_mock.h>
#include <cppkafka/mocking/offset_manager.h>

namespace cppkafka {
namespace mocking {

class KafkaCluster {
public:
    using AssignmentCallback = std::function<void(std::vector<TopicPartitionMock>&)>;
    using RevocationCallback = std::function<void(const std::vector<TopicPartitionMock>&)>;
    using MessageCallback = std::function<void(std::string topic, unsigned partition,
                                               uint64_t offset, const KafkaMessageMock*)>;

    static std::shared_ptr<KafkaCluster> make_cluster(std::string url);

    KafkaCluster(const KafkaCluster&) = delete;
    KafkaCluster& operator=(const KafkaCluster&) = delete;
    ~KafkaCluster();

    const std::string& get_url() const;

    void create_topic(const std::string& name, unsigned partitions);
    bool topic_exists(const std::string& name) const;
    void produce(const std::string& topic, unsigned partition, KafkaMessageMock message);
    template <typename Functor>
    void acquire_topic(const std::string& topic, const Functor& functor);
    KafkaTopicMock& get_topic(const std::string& name);
    const KafkaTopicMock& get_topic(const std::string& name) const;
    void subscribe(const std::string& group_id, uint64_t consumer_id,
                   const std::vector<std::string>& topics,
                   AssignmentCallback assignment_callback,
                   RevocationCallback revocation_callback,
                   MessageCallback message_callback);
    void unsubscribe(const std::string& group_id, uint64_t consumer_id);
private:
    struct ConsumerMetadata {
        using PartitionSubscriptionMap = std::unordered_map<int, KafkaPartitionMock::SubscriberId>;

        const AssignmentCallback assignment_callback;
        const RevocationCallback revocation_callback;
        const MessageCallback message_callback;
        std::vector<TopicPartitionMock> partitions_assigned;
        std::unordered_map<KafkaTopicMock*, PartitionSubscriptionMap> subscriptions;
    };

    using ConsumerSet = std::unordered_set<uint64_t>;
    using TopicConsumersMap = std::unordered_map<std::string, ConsumerSet>;

    KafkaCluster(std::string url);

    void generate_assignments(const std::string& group_id,
                              const TopicConsumersMap& topic_consumers);
    void generate_revocations(const TopicConsumersMap& topic_consumers);
    void do_unsubscribe(const std::string& group_id, uint64_t consumer_id);

    const std::string url_;
    std::shared_ptr<OffsetManager> offset_manager_;
    std::unordered_map<std::string, KafkaTopicMock> topics_;
    mutable std::mutex topics_mutex_;
    std::unordered_map<uint64_t, ConsumerMetadata> consumer_data_;
    std::unordered_map<std::string, TopicConsumersMap> group_topics_data_;
    mutable std::mutex consumer_data_mutex_;
};

template <typename Functor>
void KafkaCluster::acquire_topic(const std::string& topic, const Functor& functor) {
    std::unique_lock<std::mutex> lock(topics_mutex_);
    auto iter = topics_.find(topic);
    if (iter == topics_.end()) {
        throw std::runtime_error("Topic " + topic + " doesn't exist");
    }
    // Unlock and execute callback. We won't remove topics so this is thread safe on a
    // cluster level
    lock.unlock();
    functor(iter->second);
}

} // mocking
} // cppkafka

#endif // CPPKAFKA_MOCKING_KAFKA_CLUSTER_H