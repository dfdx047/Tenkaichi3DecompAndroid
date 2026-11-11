/* Compose the port-owned entry from the installed BT4 menu's own glyphs.
 * The original disc textures remain unchanged. */
int MenuLabels_Bt4MainItem(const char *root,const char *label,unsigned char *out[2]) {
    static const char *es[]={"Nido del Tiempo","Gira Mundial","Duelo","Entrenamiento","Palacio de Baba","Opciones"};
    static const char *en[]={"Dragon History","Time Nest","Dragon World Tour","Duel Mode","UNUSED","Evolution Z","Ultimate Training","Baba's Palace","Zuno's Temple","Options","Dragon Balls"};
    extern int Port_GameProfileEnglish(void);
    MlCtx *c=calloc(1,sizeof(*c));MlBuf main,select;int ok=0,w,h;
    out[0]=out[1]=NULL;if(!c)return 0;
    pthread_mutex_lock(&sMlCutter);
    for(int number=448;number<=449;number++){
        char path[1800];snprintf(path,sizeof(path),"%s/pzs4us1/%05d.bin",root,number);
        FILE *f=fopen(path,"rb");if(!f)goto done;
        if(fseek(f,0,SEEK_END)){fclose(f);goto done;}long n=ftell(f);
        if(n<=0||n>64*1024*1024||fseek(f,0,SEEK_SET)){fclose(f);goto done;}
        unsigned char *data=ml_alloc(c,(size_t)n);
        if(!data||fread(data,1,n,f)!=(size_t)n){fclose(f);goto done;}
        fclose(f);c->file[ml_file(number)]=(MlBuf){data,(size_t)n};
    }
    ml_cutter_clear();
    if(!ml_screen(c,448,2,&select)||!ml_screen(c,449,2,&main)||
       !ml_cutter_sheets(c,select,6,NF_STAGE,9)||!bt4_stage_style()){fprintf(stderr,"bt4 lettering: stage unavailable\n");goto done;}
    for(int k=0;k<2;k++){
        int style=NF_SONG+k;
        if(!ml_cutter_sheets(c,main,5+k,style,2))goto done;
        if(!Port_GameProfileEnglish()){
            static const int rows[]={1,2,3,6,7,9};
            unsigned char *packed=calloc(SHEET_W*SHEET_H,4);if(!packed)goto done;
            for(int j=0;j<6;j++)memcpy(packed+j*SHEET_W*32*4,sSheet[style][rows[j]/8]+(rows[j]%8)*SHEET_W*32*4,SHEET_W*32*4);
            for(int j=0;j<MAX_SHEETS;j++){free(sSheet[style][j]);sSheet[style][j]=NULL;}
            sSheet[style][0]=packed;sSheetCount[style]=1;
        }
        if(!style_build(style,Port_GameProfileEnglish()?en:es,Port_GameProfileEnglish()?11:6,32)){fprintf(stderr,"bt4 lettering: menu unavailable %d\n",k);goto done;}
        style_lend(&sStyle[style],&sStyle[NF_STAGE]);sState=3;
        NfStyle *st=&sStyle[style];
        if(!st->g['h'].px&&st->g['n'].px&&st->g['l'].px){
            NfGlyph *g=&st->g['h'],*n=&st->g['n'],*l=&st->g['l'];
            *g=*n;g->w=48;g->h=32;g->ox=0;g->oy=0;g->px=calloc(48*32,4);
            if(!g->px)goto done;
            for(int part=0;part<2;part++){NfGlyph *src=part?l:n;
                for(int y=0;y<src->h;y++)for(int x=0;x<src->w;x++){
                    int dx=x+src->ox,dy=y+src->oy;
                    if(dx>=0&&dx<48&&dy>=0&&dy<32){unsigned char *dst=g->px+(dy*48+dx)*4,*p=src->px+(y*src->w+x)*4;if(p[3]>dst[3])memcpy(dst,p,4);}
                }
            }g->sy0=l->sy0;g->sy1=n->sy1;
        }
        int baseline=st->g['e'].sy1;
        for(int ch=33;ch<127;ch++)if(st->g[ch].px){
            NfGlyph *g=&st->g[ch];int shift=baseline-g->sy1;
            if(ch=='g'||ch=='j'||ch=='p'||ch=='q'||ch=='y')shift+=3;
            g->oy+=shift;g->sy0+=shift;g->sy1+=shift;
            g->lb=g->rb=.85f;
            /* All letters share the menu row's colour ramp after baseline
               alignment, including letters borrowed from a stage name. */
            for(int gy=0;gy<g->h;gy++){
                int row=g->oy+gy;
                if(row<0||row>=st->rowH)continue;
                for(int gx=0;gx<g->w;gx++){
                    unsigned char *pixel=g->px+(gy*g->w+gx)*4;
                    for(int channel=0;channel<3;channel++)
                        pixel[channel]=(unsigned char)(st->outline[channel]+
                            ((int)st->ramp[row][channel]-st->outline[channel])*pixel[3]/255);
                }
            }
        }
        out[k]=MenuLabels_NameCompose_(style,label,&w,&h);
        if(!out[k]||w!=512||h!=32){fprintf(stderr,"bt4 lettering: compose unavailable %d\n",k);goto done;}
    }
    ok=1;
done:
    if(!ok){free(out[0]);free(out[1]);out[0]=out[1]=NULL;}
    ml_cutter_clear();pthread_mutex_unlock(&sMlCutter);ml_free(c);free(c);return ok;
}
