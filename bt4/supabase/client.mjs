// Reference client and integration-test driver. The shipped game uses the
// native adapter; no Node runtime or Supabase account UI is required in game.
export class SupabaseSignal {
  constructor(url, publishableKey, session = null) {
    const parsed = new URL(url);
    if (parsed.protocol !== 'https:' || parsed.pathname !== '/') throw Error('HTTPS_PROJECT_URL_REQUIRED');
    if (!publishableKey || publishableKey.startsWith('sb_secret_')) throw Error('PUBLISHABLE_KEY_REQUIRED');
    this.url = parsed.origin; this.key = publishableKey; this.session = session;
    this.events = []; this.waiters = []; this.serial = 0; this.closed = false;
  }
  async request(path, body, authenticated = true) {
    const response = await fetch(this.url + path, {
      method: 'POST', redirect: 'error', signal: AbortSignal.timeout(10000),
      headers: {apikey:this.key,'Content-Type':'application/json',
        ...(authenticated ? {Authorization:'Bearer '+this.session.access_token} : {})},
      body: JSON.stringify(body)
    });
    const result = await response.json();
    if (!response.ok) throw Error('SUPABASE_HTTP_'+response.status+':'+(result.error_code || result.code || 'REQUEST_FAILED'));
    return result;
  }
  async authenticate() {
    if (this.session && this.session.expires_at > Date.now()/1000+90) return;
    const refreshing = Boolean(this.session?.refresh_token);
    this.session = await this.request(refreshing ? '/auth/v1/token?grant_type=refresh_token' : '/auth/v1/signup',
      refreshing ? {refresh_token:this.session.refresh_token} : {data:{client:'BT4decomp'}}, false);
    if (!this.session.access_token || !this.session.user?.id) throw Error('AUTH_REQUIRED');
  }
  async rpc(operation, args = {}) {
    await this.authenticate();
    return this.request('/rest/v1/rpc/bt4_api', {p_op:operation,p_args:args});
  }
  wire(event,payload,topic=this.topic) {
    const ref = String(++this.serial);
    this.socket.send(JSON.stringify({topic,event,payload,ref,join_ref:this.joinRef}));
    return ref;
  }
  async connect(topicOwner = null) {
    await this.authenticate();
    this.closed = false;
    this.topic = 'realtime:bt4:inbox:'+(topicOwner || this.session.user.id);
    this.socket = new WebSocket(this.url.replace(/^https:/,'wss:')+'/realtime/v1/websocket?apikey='+encodeURIComponent(this.key)+'&vsn=1.0.0');
    const joined = new Promise((resolve,reject)=>{this.joinResolve=resolve;this.joinReject=reject;});
    this.socket.onopen=()=>{
      this.joinRef=String(this.serial+1);
      this.joinRequest=this.wire('phx_join',{config:{private:true,broadcast:{ack:false,self:false},presence:{enabled:false},postgres_changes:[]},access_token:this.session.access_token});
    };
    this.socket.onmessage=event=>{
      let packet; try {packet=JSON.parse(event.data);} catch {return;}
      if (packet.event==='phx_reply' && packet.ref===this.joinRequest) {
        if(packet.payload?.status==='ok') this.joinResolve(); else this.joinReject(Error('PRIVATE_CHANNEL_DENIED'));
      } else if(packet.event==='broadcast') {
        const item={event:packet.payload.event,body:packet.payload.payload};
        this.events.push(item);
        if(this.events.length>512) {this.disconnect();return;}
        for(const wake of this.waiters.splice(0)) wake();
      }
    };
    this.socket.onerror=()=>this.joinReject(Error('REALTIME_CONNECTION_FAILED'));
    this.socket.onclose=()=>{this.closed=true;this.joinReject(Error('REALTIME_CLOSED'));for(const wake of this.waiters.splice(0))wake();};
    const timeout=setTimeout(()=>this.joinReject(Error('REALTIME_JOIN_TIMEOUT')),10000);
    try {await joined;} catch(error){this.disconnect();throw error;} finally{clearTimeout(timeout);}
    this.heartbeat=setInterval(()=>{
      if(this.socket.readyState===WebSocket.OPEN)this.wire('heartbeat',{},'phoenix');
    },20000);
    this.refresh=setInterval(async()=>{
      try {
        await this.authenticate();
        if(this.socket.readyState===WebSocket.OPEN)this.wire('access_token',{access_token:this.session.access_token});
      } catch {this.disconnect();}
    },60000);
  }
  async wait(event, predicate = () => true, timeout = 10000) {
    const deadline=Date.now()+timeout;
    while(Date.now()<deadline) {
      const index=this.events.findIndex(x=>x.event===event && predicate(x.body));
      if(index>=0)return this.events.splice(index,1)[0].body;
      if(this.closed)throw Error('REALTIME_CLOSED');
      await new Promise(resolve=>{
        const wake=()=>{clearTimeout(timer);const i=this.waiters.indexOf(wake);if(i>=0)this.waiters.splice(i,1);resolve();};
        const timer=setTimeout(wake,Math.max(1,deadline-Date.now()));this.waiters.push(wake);
      });
    }
    throw Error('EVENT_TIMEOUT:'+event);
  }
  disconnect() {
    clearInterval(this.heartbeat);clearInterval(this.refresh);
    this.closed=true;this.socket?.close();
    for(const wake of this.waiters.splice(0))wake();
  }
}
