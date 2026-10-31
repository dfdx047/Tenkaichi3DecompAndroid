import unittest
from verify_disc_startup import disc_bind_boundary,disc_search_completed

class DiscBoundaryTests(unittest.TestCase):
    def test_requires_exact_request_and_prior_owned_semaphore(self):
        r={'disc_bind_semaphores':1,'selector':0x77,'pc':0x32F028,'target':0,'native_iop_error':'',
           'packet_words':[0]*6+[42,0x3A8780,0x80000597]}
        self.assertTrue(disc_bind_boundary(r))
        for key,value in [('disc_bind_semaphores',0),('selector',0x40),('pc',0x32EC68),('target',0x2FD500),
                          ('native_iop_error','earlier failure'),('packet_words',[]),
                          ('packet_words',[0]*6+[41,0x3A8780,0x80000597]),
                          ('packet_words',[0]*6+[42,0x3A8780,0x80000592])]:
            with self.subTest(key=key,value=value):self.assertFalse(disc_bind_boundary(dict(r,**{key:value})))

    def test_completed_search_does_not_hide_transport_or_runtime_failure(self):
        r={'disc_rpc_binds':1,'disc_rpc_calls':1,'disc_result':1}
        self.assertTrue(disc_search_completed(r))
        for key,value in [('disc_rpc_binds',0),('disc_rpc_calls',0),('disc_rpc_calls',7),('disc_result',0),('native_iop_error','failed')]:
            with self.subTest(key=key,value=value):self.assertFalse(disc_search_completed(dict(r,**{key:value})))
