/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef ONEFLOW_CORE_COMM_NETWORK_IBVERBS_IBVERBS_QP_H_
#define ONEFLOW_CORE_COMM_NETWORK_IBVERBS_IBVERBS_QP_H_

#include <cstdint>
#include <deque>
#include <memory>
#include "oneflow/core/comm_network/ibverbs/ibverbs_memory_desc.h"
#include "oneflow/core/actor/actor_message.h"
#include "oneflow/core/platform/include/ibv.h"

#include <infiniband/verbs.h>

#if defined(WITH_RDMA) && defined(OF_PLATFORM_POSIX)

namespace oneflow {

class ActorMsgMR final {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ActorMsgMR);
  ActorMsgMR() = delete;
  ActorMsgMR(ibv_mr *   mr, char * addr, size_t  size):size_(size){
    msg_ = reinterpret_cast<ActorMsg*>(addr); //这里没有问题
    mr_.reset(mr);
  }
  ~ActorMsgMR() {
    mr_.reset();
  }

  char * addr() { return reinterpret_cast<char *>(msg_) ; } //这个是没错的
  uint32_t size() {return size_ ;}
  uint32_t lkey() { return mr_->lkey ; }
  ActorMsg  msg()  { return *msg_;} //这个函数也没问题
  //这个函数也没问题
  void set_msg(const ActorMsg& val) {
    std::cout<<"in set_msg of ActorMsgMR, the val comm_net_sequence_number:" << val.comm_net_sequence_number() << std::endl;
    *msg_ = val ;
  }

 private:
    size_t size_;
    std::shared_ptr<ibv_mr> mr_;
    ActorMsg *  msg_;
};

class IBVerbsQP;

struct WorkRequestId {
  IBVerbsQP* qp;
  int32_t outstanding_sge_cnt;
  void* read_id;
  ActorMsgMR* msg_mr;
};

class MessagePool final {
  public:
    OF_DISALLOW_COPY_AND_MOVE(MessagePool);
    MessagePool() = delete; //todo:这里可能要修改
    //析构函数
    ~MessagePool() {
      // while(message_buf_.empty() == false) {
      //   delete message_buf_.front();
      //   message_buf_.pop_front();
      // }
    }//todo:这里可能要修改

    MessagePool(ibv_pd* pd, uint32_t number_of_message):num_of_message_(number_of_message) {
      pd_.reset(pd);
      RegisterMessagePool();
    }
    //以后这里可以切割内存，注册一块大的，再不断的分割
    void RegisterMessagePool();
    // void RegisterMessagePool(){
    //   ActorMsg msg;
    //   size_t ActorMsgSize = sizeof(msg);
    //   std::cout<<"ActorMsgSize:"<<ActorMsgSize << std::endl;
    //   size_t RegisterMemorySize  = ActorMsgSize  * (num_of_message_);
    //   char * addr =(char*) malloc(RegisterMemorySize );
    //   ibv_mr *   mr =ibv::wrapper.ibv_reg_mr_wrap(
    //       pd_.get(),  addr, RegisterMemorySize,
    //       IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);
    //   CHECK(mr);
    //   for(size_t i = 0;  i < num_of_message_ ; i++){
    //       char * split_addr =addr + ActorMsgSize * i ; //这里切割地址没有问题
    //       ActorMsgMR * msg_mr = new ActorMsgMR(mr,split_addr, ActorMsgSize);
    //       message_buf_.push_front(msg_mr);
    //   }
    // }
    ActorMsgMR *  GetMessage();
    ActorMsgMR * GetMessageFromBuf();
    void PutMessage(ActorMsgMR * msg_mr);
    
    std::deque<ActorMsgMR*> GetMessageBuf() {
      return message_buf_;
    }

    bool isEmpty() {
      std::unique_lock<std::mutex>  msg_buf_lck(message_buf_mutex_);
      return message_buf_.empty();
    }

  private:
//    ibv_pd* pd_;
    std::shared_ptr<ibv_pd> pd_;
    size_t  num_of_message_;
    std::mutex message_buf_mutex_;
    std::deque<ActorMsgMR*> message_buf_;
};

struct IBVerbsCommNetRMADesc;

class IBVerbsQP final {
 public:
  OF_DISALLOW_COPY_AND_MOVE(IBVerbsQP);
  IBVerbsQP() = delete;
  IBVerbsQP(ibv_context*, ibv_pd*, uint8_t port_num, ibv_cq* send_cq, ibv_cq* recv_cq);
  IBVerbsQP(ibv_context *, ibv_pd*, uint8_t port_num, ibv_cq * send_cq, ibv_cq* recv_cq,
            std::shared_ptr<MessagePool> recv_msg_buf, std::shared_ptr<MessagePool> send_msg_buf);
  ~IBVerbsQP();

  uint32_t qp_num() const { return qp_->qp_num; }
  void Connect(const IBVerbsConnectionInfo& peer_info);
  void PostAllRecvRequest();

  void PostReadRequest(const IBVerbsCommNetRMADesc& remote_mem, const IBVerbsMemDesc& local_mem,
                       void* read_id);
  void PostSendRequest(const ActorMsg& msg);

  void ReadDone(WorkRequestId*);
  void SendDone(WorkRequestId*);
  void RecvDone(WorkRequestId*);

 private:
  void EnqueuePostSendReadWR(ibv_send_wr wr, ibv_sge sge);
  void PostPendingSendWR();
  WorkRequestId* NewWorkRequestId();
  void DeleteWorkRequestId(WorkRequestId* wr_id);
  ActorMsgMR* GetOneSendMsgMRFromBuf();
  void PostRecvRequest(ActorMsgMR*);

  ibv_context* ctx_;
  ibv_pd* pd_;
  uint8_t port_num_;
  ibv_qp* qp_;

  std::mutex pending_send_wr_mutex_;
  uint32_t num_outstanding_send_wr_;
  uint32_t max_outstanding_send_wr_;
  std::queue<std::pair<ibv_send_wr, ibv_sge>> pending_send_wr_queue_;
 
  std::shared_ptr<MessagePool> recv_msg_buf_;
  std::shared_ptr<MessagePool> send_msg_buf_;
 };

}  // namespace oneflow

#endif  // WITH_RDMA && OF_PLATFORM_POSIX

#endif  // ONEFLOW_CORE_COMM_NETWORK_IBVERBS_IBVERBS_QP_H_
