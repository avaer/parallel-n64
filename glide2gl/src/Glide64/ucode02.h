/*
* Glide64 - Glide video plugin for Nintendo 64 emulators.
* Copyright (c) 2002  Dave2001
* Copyright (c) 2003-2009  Sergey 'Gonetz' Lipski
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

//****************************************************************
//
// Glide64 - Glide Plugin for Nintendo 64 emulators
// Project started on December 29th, 2001
//
// Authors:
// Dave2001, original author, founded the project in 2001, left it in 2002
// Gugaman, joined the project in 2002, left it in 2002
// Sergey 'Gonetz' Lipski, joined the project in 2002, main author since fall of 2002
// Hiroshi 'KoolSmoky' Morii, joined the project in 2007
//
//****************************************************************
//
// To modify Glide64:
// * Write your name and (optional)email, commented by your work, so I know who did it, and so that you can find which parts you modified when it comes time to send it to me.
// * Do NOT send me the whole project or file that you modified.  Take out your modified code sections, and tell me where to put them.  If people sent the whole thing, I would have many different versions, but no idea how to combine them all.
//
//****************************************************************

static void calc_point_light (VERTEX *v, float * vpos)
{
   uint32_t l;
   float light_intensity, color[3];

   light_intensity = 0.0f;
   color[0] = rdp.light[rdp.num_lights].col[0];
   color[1] = rdp.light[rdp.num_lights].col[1];
   color[2] = rdp.light[rdp.num_lights].col[2];

   for (l = 0; l < rdp.num_lights; l++)
   {
      light_intensity = 0.0f;

      if (rdp.light[l].nonblack)
      {
         float lvec[3], light_len2, light_len, at;
         lvec[0] = rdp.light[l].x - vpos[0];
         lvec[1] = rdp.light[l].y - vpos[1];
         lvec[2] = rdp.light[l].z - vpos[2];

         light_len2 = lvec[0] * lvec[0] + lvec[1] * lvec[1] + lvec[2] * lvec[2];
         light_len = sqrtf(light_len2);
         //FRDP ("calc_point_light: len: %f, len2: %f\n", light_len, light_len2);
         at = rdp.light[l].ca + light_len/65535.0f*rdp.light[l].la + light_len2/65535.0f*rdp.light[l].qa;

         if (at > 0.0f)
            light_intensity = 1/at;//DotProduct (lvec, nvec) / (light_len * normal_len * at);
      }

      if (light_intensity > 0.0f)
      {
         color[0] += rdp.light[l].col[0] * light_intensity;
         color[1] += rdp.light[l].col[1] * light_intensity;
         color[2] += rdp.light[l].col[2] * light_intensity;
      }
   }

   if (color[0] > 1.0f)
      color[0] = 1.0f;
   if (color[1] > 1.0f)
      color[1] = 1.0f;
   if (color[2] > 1.0f)
      color[2] = 1.0f;

   v->r = (uint8_t)(color[0]*255.0f);
   v->g = (uint8_t)(color[1]*255.0f);
   v->b = (uint8_t)(color[2]*255.0f);
}

static void uc2_vertex(uint32_t w0, uint32_t w1)
{
   uint32_t i, l, addr, geom_mode;
   int v0, n;
   float x, y, z;
   
   if (!(w0 & 0x00FFFFFF))
   {
      uc6_obj_rectangle(w0, w1);
      return;
   }

   // This is special, not handled in update(), but here
   // * Matrix Pre-multiplication idea by Gonetz (Gonetz@ngs.ru)
   if (rdp.update & UPDATE_MULT_MAT)
   {
      rdp.update ^= UPDATE_MULT_MAT;
      MulMatrices(rdp.model, rdp.proj, rdp.combined);
   }
   if (rdp.update & UPDATE_LIGHTS)
   {
      rdp.update ^= UPDATE_LIGHTS;

      // Calculate light vectors
      for (l = 0; l < rdp.num_lights; l++)
      {
         InverseTransformVector(&rdp.light[l].dir[0], rdp.light_vector[l], rdp.model);
         NormalizeVector (rdp.light_vector[l]);
      }
   }

   addr = segoffset(w1);

   n = (w0 >> 12) & 0xFF;
   v0 = ((w0 >> 1) & 0x7F) - n;

   if (v0 < 0)
      return;

   geom_mode = rdp.geom_mode;
   if ((settings.hacks&hack_Fzero) && (rdp.geom_mode & G_TEXTURE_GEN))
   {
      if (((int16_t*)gfx_info.RDRAM)[(((addr) >> 1) + 4)^1] || ((int16_t*)gfx_info.RDRAM)[(((addr) >> 1) + 5)^1])
         rdp.geom_mode ^= G_TEXTURE_GEN;
   }

   gSPVertex(addr, n, v0);

   rdp.geom_mode = geom_mode;
}

static void uc2_modifyvtx(uint32_t w0, uint32_t w1)
{
   uint8_t where = (uint8_t)((w0 >> 16) & 0xFF);
   uint16_t vtx = (uint16_t)((w0 >> 1) & 0xFFFF);

   //FRDP ("uc2:modifyvtx: vtx: %d, where: 0x%02lx, val: %08lx - ", vtx, where, w1);
   uc0_modifyvtx(where, vtx, w1);
}

static void uc2_culldl(uint32_t w0, uint32_t w1)
{
   uint16_t i, vStart, vEnd, cond;

   vStart = (uint16_t)(w0 & 0xFFFF) >> 1;
   vEnd = (uint16_t)(w1 & 0xFFFF) >> 1;
   cond = 0;
   //FRDP ("uc2:culldl start: %d, end: %d\n", vStart, vEnd);

   if (vEnd < vStart)
      return;

   for (i = vStart; i <= vEnd; i++)
   {
#if 0
      VERTEX *v;

      v = (VERTEX*)&rdp.vtx[i];

      /*
       * Check if completely off the screen
       * (quick frustrum clipping for 90 FOV)
       */

      if (v->x >= -v->w)
         cond |= X_CLIP_MAX;
      if (v->x <= v->w)
         cond |= X_CLIP_MIN;
      if (v->y >= -v->w)
         cond |= Y_CLIP_MAX;
      if (v->y <= v->w)
         cond |= Y_CLIP_MIN;
      if (v->w >= 0.1f)
         cond |= Z_CLIP_MAX;

      if (cond == 0x1F)
         return;
#endif

      cond |= (~rdp.vtx[i].scr_off) & 0x1F;
      if (cond == 0x1F)
         return;
   }

   //LRDP(" - ");  // specify that the enddl is not a real command
   uc0_enddl(w0, w1);
}

static void uc2_tri1(uint32_t w0, uint32_t w1)
{
   VERTEX *v[3];
   if ((w0 & 0x00FFFFFF) == 0x17)
   {
      uc6_obj_loadtxtr(w0, w1);
      return;
   }

   if (rdp.skip_drawing)
      return;

   v[0] = &rdp.vtx[_SHIFTR(w0,  17, 7)];
   v[1] = &rdp.vtx[_SHIFTR(w0,   9, 7)];
   v[2] = &rdp.vtx[_SHIFTR(w0,   1, 7)];

   cull_trianglefaces(v, 1, true, true, 0);
}

static void uc2_quad(uint32_t w0, uint32_t w1)
{
   VERTEX *v[6];
   if ((w0 & 0x00FFFFFF) == 0x2F)
   {
      uint32_t command = w0 >> 24;
      if (command == 0x6)
      {
         uc6_obj_ldtx_sprite(w0, w1);
         return;
      }
      if (command == 0x7)
      {
         uc6_obj_ldtx_rect(w0, w1);
         return;
      }
   }

   if (rdp.skip_drawing)
      return;

   v[0] = &rdp.vtx[_SHIFTR(w0, 17, 7)];
   v[1] = &rdp.vtx[_SHIFTR(w0,  9, 7)];
   v[2] = &rdp.vtx[_SHIFTR(w0,  1, 7)];
   v[3] = &rdp.vtx[_SHIFTR(w1, 17, 7)];
   v[4] = &rdp.vtx[_SHIFTR(w1,  9, 7)];
   v[5] = &rdp.vtx[_SHIFTR(w1,  1, 7)];

   cull_trianglefaces(v, 2, true, true, 0);
}

static void uc2_line3d(uint32_t w0, uint32_t w1)
{
   if ( (w0 & 0xFF) == 0x2F )
      uc6_ldtx_rect_r(w0, w1);
   else
   {
      VERTEX *v[3];
      uint32_t cull_mode;
      uint16_t width;

      v[0] = &rdp.vtx[(w0 >> 17) & 0x7F];
      v[1] = &rdp.vtx[(w0 >> 9) & 0x7F];
      v[2] = &rdp.vtx[(w0 >> 9) & 0x7F];
      width = (uint16_t)(w0 + 3)&0xFF;
      cull_mode = (rdp.flags & CULLMASK) >> CULLSHIFT;
      rdp.flags |= CULLMASK;
      rdp.update |= UPDATE_CULL_MODE;
      cull_trianglefaces(v, 1, true, true, width);
      rdp.flags ^= CULLMASK;
      rdp.flags |= cull_mode << CULLSHIFT;
      rdp.update |= UPDATE_CULL_MODE;
   }
}

static void uc2_special3(uint32_t w0, uint32_t w1)
{
   LRDP("uc2:special3\n");
}

static void uc2_special2(uint32_t w0, uint32_t w1)
{
   LRDP("uc2:special2\n");
}

static void uc2_dma_io(uint32_t w0, uint32_t w1)
{
   LRDP("uc2:dma_io\n");
}

static void uc2_pop_matrix(uint32_t w0, uint32_t w1)
{
   // Just pop the modelview matrix
   //   // Just pop the modelview matrix
   modelview_pop (w1 >> 6);
   //FRDP ("uc2:pop_matrix %08lx, %08lx\n", w0, w1);
}

static void uc2_geom_mode(uint32_t w0, uint32_t w1)
{
   // Switch around some things
   uint32_t clr_mode = (w0 & 0x00DFC9FF) |
      ((w0 & 0x00000600) << 3) |
      ((w0 & 0x00200000) >> 12) | 0xFF000000;
   uint32_t set_mode = (w1 & 0xFFDFC9FF) |
      ((w1 & 0x00000600) << 3) |
      ((w1 & 0x00200000) >> 12);

   rdp.geom_mode &= clr_mode;
   rdp.geom_mode |= set_mode;

   if (rdp.geom_mode & G_ZBUFFER)
   {
      if (!(rdp.flags & ZBUF_ENABLED))
      {
         rdp.flags |= ZBUF_ENABLED;
         rdp.update |= UPDATE_ZBUF_ENABLED;
      }
   }
   else
   {
      if ((rdp.flags & ZBUF_ENABLED))
      {
         if (!settings.flame_corona || (rdp.rm != 0x00504341)) //hack for flame's corona
            rdp.flags ^= ZBUF_ENABLED;
         rdp.update |= UPDATE_ZBUF_ENABLED;
      }
   }

   if (rdp.geom_mode & CULL_FRONT)
   {
      if (!(rdp.flags & CULL_FRONT))
      {
         rdp.flags |= CULL_FRONT;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }
   else
   {
      if (rdp.flags & CULL_FRONT)
      {
         rdp.flags ^= CULL_FRONT;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }
   if (rdp.geom_mode & CULL_BACK)
   {
      if (!(rdp.flags & CULL_BACK))
      {
         rdp.flags |= CULL_BACK;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }
   else
   {
      if (rdp.flags & CULL_BACK)
      {
         rdp.flags ^= CULL_BACK;
         rdp.update |= UPDATE_CULL_MODE;
      }
   }

   if (rdp.geom_mode & G_FOG)
   {
      if (!(rdp.flags & FOG_ENABLED))
      {
         rdp.flags |= FOG_ENABLED;
         rdp.update |= UPDATE_FOG_ENABLED;
      }
   }
   else
   {
      if (rdp.flags & FOG_ENABLED)
      {
         rdp.flags ^= FOG_ENABLED;
         rdp.update |= UPDATE_FOG_ENABLED;
      }
   }
}

static void uc2_matrix(uint32_t w0, uint32_t w1)
{
   DECLAREALIGN16VAR(m[4][4]);
   uint8_t command;

   if (!(w0 & 0x00FFFFFF))
   {
      uc6_obj_rectangle_r(w0, w1);
      return;
   }
   LRDP("uc2:matrix\n");

   load_matrix(m, segoffset(w1));

   command = (uint8_t)((w0 ^ 1) & 0xFF);
   switch (command)
   {
      case 0: // modelview mul nopush
         LRDP("modelview mul\n");
         modelview_mul (m);
         break;

      case 1: // modelview mul push
         LRDP("modelview mul push\n");
         modelview_mul_push (m);
         break;

      case 2: // modelview load nopush
         LRDP("modelview load\n");
         modelview_load (m);
         break;

      case 3: // modelview load push
         LRDP("modelview load push\n");
         modelview_load_push (m);
         break;

      case 4: // projection mul nopush
      case 5: // projection mul push, can't push projection
         LRDP("projection mul\n");
         projection_mul (m);
         break;

      case 6: // projection load nopush
      case 7: // projection load push, can't push projection
         LRDP("projection load\n");
         projection_load (m);
         break;

      default:
         FRDP_E ("Unknown matrix command, %02lx", command);
         FRDP ("Unknown matrix command, %02lx", command);
   }

#ifdef EXTREME_LOGGING
   FRDP ("{%f,%f,%f,%f}\n", m[0][0], m[0][1], m[0][2], m[0][3]);
   FRDP ("{%f,%f,%f,%f}\n", m[1][0], m[1][1], m[1][2], m[1][3]);
   FRDP ("{%f,%f,%f,%f}\n", m[2][0], m[2][1], m[2][2], m[2][3]);
   FRDP ("{%f,%f,%f,%f}\n", m[3][0], m[3][1], m[3][2], m[3][3]);
   FRDP ("\nmodel\n{%f,%f,%f,%f}\n", rdp.model[0][0], rdp.model[0][1], rdp.model[0][2], rdp.model[0][3]);
   FRDP ("{%f,%f,%f,%f}\n", rdp.model[1][0], rdp.model[1][1], rdp.model[1][2], rdp.model[1][3]);
   FRDP ("{%f,%f,%f,%f}\n", rdp.model[2][0], rdp.model[2][1], rdp.model[2][2], rdp.model[2][3]);
   FRDP ("{%f,%f,%f,%f}\n", rdp.model[3][0], rdp.model[3][1], rdp.model[3][2], rdp.model[3][3]);
   FRDP ("\nproj\n{%f,%f,%f,%f}\n", rdp.proj[0][0], rdp.proj[0][1], rdp.proj[0][2], rdp.proj[0][3]);
   FRDP ("{%f,%f,%f,%f}\n", rdp.proj[1][0], rdp.proj[1][1], rdp.proj[1][2], rdp.proj[1][3]);
   FRDP ("{%f,%f,%f,%f}\n", rdp.proj[2][0], rdp.proj[2][1], rdp.proj[2][2], rdp.proj[2][3]);
   FRDP ("{%f,%f,%f,%f}\n", rdp.proj[3][0], rdp.proj[3][1], rdp.proj[3][2], rdp.proj[3][3]);
#endif
}

static void uc2_moveword(uint32_t w0, uint32_t w1)
{
   uint16_t offset = (uint16_t)(w0 & 0xFFFF);

   switch (_SHIFTR( w0, 16, 8))
   {
      case G_MW_MATRIX:
         // NOTE: right now it's assuming that it sets the integer part first. This could
         // be easily fixed, but only if i had something to test with.
         
         // do matrix pre-mult so it's re-updated next time
         if (rdp.update & UPDATE_MULT_MAT)
         {
            rdp.update ^= UPDATE_MULT_MAT;
            MulMatrices(rdp.model, rdp.proj, rdp.combined);
         }

         if (w0 & 0x20) // fractional part
         {
            float fpart;
            int index_x = (w0 & 0x1F) >> 1;
            int index_y = index_x >> 2;
            index_x &= 3;

            fpart = (w1>>16)/65536.0f;
            rdp.combined[index_y][index_x] = (float)(int)rdp.combined[index_y][index_x];
            rdp.combined[index_y][index_x] += fpart;

            fpart = (w1&0xFFFF)/65536.0f;
            rdp.combined[index_y][index_x+1] = (float)(int)rdp.combined[index_y][index_x+1];
            rdp.combined[index_y][index_x+1] += fpart;
         }
         else
         {
            int index_x = (w0 & 0x1F) >> 1;
            int index_y = index_x >> 2;
            index_x &= 3;

            rdp.combined[index_y][index_x] = (short)(w1>>16);
            rdp.combined[index_y][index_x+1] = (short)(w1&0xFFFF);
         }
         break;

      case G_MW_NUMLIGHT:
         rdp.num_lights = w1 / 24;
         rdp.update |= UPDATE_LIGHTS;
         break;
      case G_MW_CLIP:
         if (offset == 0x04)
         {
            rdp.clip_ratio = (float)vi_integer_sqrt(w1);
            rdp.update |= UPDATE_VIEWPORT;
         }
         break;
      case G_MW_SEGMENT:
         if ((w1 & BMASK) < BMASK)
            rdp.segment[(offset >> 2) & 0xF] = w1;
         break;
      case G_MW_FOG:
         rdp.fog_multiplier = (short)(w1 >> 16);
         rdp.fog_offset = (short)(w1 & 0x0000FFFF);

         //offset must be 0 for move_fog, but it can be non zero in Nushi Zuri 64 - Shiokaze ni Notte
         //low-level display list has setothermode commands in this place, so this is obviously not move_fog.
         if (offset == 0x04)
            rdp.tlut_mode = (w1 == 0xffffffff) ? 0 : 2;
         break;

      case G_MW_LIGHTCOL:
         {
            int n = offset / 24;

            rdp.light[n].col[0] = (float)((w1 >> 24) & 0xFF) / 255.0f;
            rdp.light[n].col[1] = (float)((w1 >> 16) & 0xFF) / 255.0f;
            rdp.light[n].col[2] = (float)((w1 >> 8) & 0xFF) / 255.0f;
            rdp.light[n].col[3] = 255;
         }
         break;

      case G_MW_FORCEMTX:
         /* Handled in movemem */
         break;
      case G_MW_PERSPNORM:
         /* implement something here? */
         break;
   }
}

static void uc2_movemem(uint32_t w0, uint32_t w1)
{
   int16_t *rdram     = (int16_t*)(gfx_info.RDRAM  + RSP_SegmentToPhysical(w1));

   switch (_SHIFTR(w0, 0, 8))
   {
      case 0:
      case 2:
         uc6_obj_movemem(w0, w1);
         break;
      case F3DEX2_MV_VIEWPORT:
         gSPViewport( w1 );
         break;
      case G_MV_MATRIX:
         // do not update the combined matrix!
         rdp.update &= ~UPDATE_MULT_MAT;
         load_matrix(rdp.combined, segoffset(w1));
         break;

      case G_MV_LIGHT:
         {
            const uint32_t offset = (_SHIFTR( w0, 5, 11)) & 0x7F8;
            uint32_t n = offset / 24;

            if (n < 2)
               gSPLookAt_G64(w1, n);
            else
               gSPLight_G64(w1, n - 1);
         }
         break;

   }
}

static void uc2_load_ucode(uint32_t w0, uint32_t w1)
{
   LRDP("uc2:load_ucode\n");
}

static void uc2_rdphalf_2(uint32_t w0, uint32_t w1)
{
   LRDP("uc2:rdphalf_2\n");
}

static void uc2_dlist_cnt(uint32_t w0, uint32_t w1)
{
   uint32_t addr = segoffset(w1) & BMASK;
   int count = w0 & 0x000000FF;

   if (rdp.pc_i >= 9 || addr == 0)
      return;

   rdp.pc_i ++;  // go to the next PC in the stack
   rdp.pc[rdp.pc_i] = addr;  // jump to the address
   rdp.dl_count = count + 1;
   FRDP ("dl_count - addr: %08lx, count: %d\n", addr, count);
}
