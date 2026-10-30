// 按操作顺序提交参数，只合并同一操作内尚未发送的增量，避免覆盖宿主自动化。
export class NativeStateQueue {
  constructor(send, onState, onError) {
    this.send = send;
    this.onState = onState;
    this.onError = onError;
    this.pending = 0;
    this.tail = Promise.resolve();
    this.batch = null;
  }

  enqueue(command, data, beforeSend) {
    this.pending++;
    const task = this.tail.then(() => {
      beforeSend?.();
      return this.send(command, data);
    }).then(state => {
      if (this.pending === 1 && state?.values) this.onState(state);
      return state;
    }).catch(error => this.onError(error)).finally(() => { this.pending--; });
    this.tail = task;
    return task;
  }

  command(command, data = {}) {
    this.batch = null;
    return this.enqueue(command, data);
  }

  patch(changes) {
    if (this.batch) {
      const values = { ...this.batch.data.values, ...changes.values };
      Object.assign(this.batch.data, changes);
      this.batch.data.values = values;
      return this.batch.task;
    }
    const batch = { data: { ...changes, values: { ...changes.values } } };
    this.batch = batch;
    batch.task = this.enqueue('patch', batch.data, () => {
      if (this.batch === batch) this.batch = null;
    });
    return batch.task;
  }
}
