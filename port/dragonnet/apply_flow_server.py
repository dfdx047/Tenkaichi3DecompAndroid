#!/usr/bin/env python3
"""Adiciona o flow:2 (revanche) ao ranked.js do Worker C.
Uso (na raiz do repo):  python3 port/dragonnet/apply_flow_server.py          (teste)
                        python3 port/dragonnet/apply_flow_server.py --apply  (aplica)
Falha sem alterar nada se algum trecho nao for achado exatamente 1 vez."""
import os, sys
T = "port/dragonnet/ranked.js"
NEED = ["port/dragonnet/ranked_flow.js", "port/dragonnet/ranked-flow-schema.sql"]
R = [
("import", 'import {versionAllowed} from "./versions.js";\n',
 'import {versionAllowed} from "./versions.js";\nimport {flowQueue,flowView,flowExpire,flowRequest} from \'./ranked_flow.js\';\n'),
("matchView", 'rematches:m.rematch_caps===3?1:0,delta:',
 'rematches:m.rematch_caps===3?1:0,flow:await flowView(db,m),delta:'),
("expire", ' if(!p.active_match)return;\n await db.batch([',
 ' if(!p.active_match)return;\n await flowExpire(db,await one(db,\'SELECT * FROM ranked_matches WHERE id=?\',p.active_match),now);\n await db.batch(['),
("capabilities", ' const db=env.DB;\n',
 ' const db=env.DB;\n if(path===\'capabilities\')return json({lifecycle:2,connect_timeout_ms:7000,rematch_timeout_ms:15000});\n'),
("flow-insert", 'UPDATE ranked_players SET active_match=? WHERE id=? AND active_match IS NULL AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)",mine,p.id,mine)',
 'UPDATE ranked_players SET active_match=? WHERE id=? AND active_match IS NULL AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)",mine,p.id,mine),\n    stmt(db,"INSERT OR IGNORE INTO ranked_flow(match_id,host_cap) SELECT ?,? WHERE EXISTS(SELECT 1 FROM ranked_matches WHERE id=?)",mine,body.flow===2?2:0,mine)'),
("flow-guest", 'stmt(db,"UPDATE ranked_matches SET state=\'superseded\' WHERE id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND guest_id=?)",m.id,other.id,p.id)',
 'stmt(db,"UPDATE ranked_matches SET state=\'superseded\' WHERE id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND guest_id=?)",m.id,other.id,p.id),\n     stmt(db,"UPDATE ranked_flow SET guest_cap=?,paired_at=? WHERE match_id=? AND EXISTS(SELECT 1 FROM ranked_matches WHERE id=? AND guest_id=?)",body.flow===2?2:0,now,other.id,other.id,p.id)'),
("flowQueue", '",p.active_match);\n  return json(await matchView(db,m,p.id));',
 '",p.active_match);\n  await flowQueue(db,m,p,body);\n  return json(await matchView(db,m,p.id));'),
("lifecycle", ' if(!m || (m.host_id!==p.id && m.guest_id!==p.id))return json({error:"match"},404);\n',
 ' if(!m || (m.host_id!==p.id && m.guest_id!==p.id))return json({error:"match"},404);\n await flowExpire(db,m,now);m=await one(db,\'SELECT * FROM ranked_matches WHERE id=?\',m.id);\n if(path===\'lifecycle\'){const r=await flowRequest(db,m,p,body,now,matchView);return json(r.body,r.status??200);}\n'),
("cancel-leave", ' if(path==="cancel"){\n',
 ' if(path==="cancel"){\n  if(m.state===\'final\'&&await flowView(db,m)===2)await flowRequest(db,m,p,{action:\'leave\'},now,matchView);\n'),
("cancel-started", 'host_score IS NULL AND guest_score IS NULL",m.id)',
 'host_score IS NULL AND guest_score IS NULL AND NOT EXISTS(SELECT 1 FROM ranked_flow WHERE match_id=? AND started_mask<>0)",m.id,m.id)'),
]
def main():
    apply = "--apply" in sys.argv
    for f in NEED + [T]:
        if not os.path.isfile(f):
            print("FALTA:", f); return 1
    raw = open(T, "rb").read().decode("utf-8")
    crlf = "\r\n" in raw
    txt = raw.replace("\r\n", "\n")
    if "ranked_flow.js" in txt:
        print("ranked.js ja tem o flow. Nada feito."); return 1
    bad = 0
    for name, a, b in R:
        n = txt.count(a)
        print(("OK   " if n == 1 else "FALHA"), name, "(achado %d vez)" % n)
        bad += n != 1
    if bad:
        print("\n%d trecho(s) nao batem. NADA foi alterado." % bad); return 1
    if not apply:
        print("\nTeste OK. Rode com --apply para aplicar."); return 0
    for name, a, b in R:
        txt = txt.replace(a, b, 1)
    if crlf: txt = txt.replace("\n", "\r\n")
    open(T, "wb").write(txt.encode("utf-8"))
    print("\nAplicado em", T); return 0
sys.exit(main())
